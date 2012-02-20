#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include "driver.h"
#include "types.h"
#include "netplay.h"
#include "server.h"
#include "utils/md5.h"


#define MIN(a,b) (((a)<(b))?(a):(b))

#define N_LOGINLEN   0x1000
#define N_LOGIN      0x2000
#define N_COMMAND    0x4000
#define N_UPDATEDATA 0x5000

struct Client {
	char name[NETPLAY_MAX_NAME_LEN];
	int  id;
	bool disconnecting;

	FCEUD_ServerSocket *socket;

	//store incoming data until processed
	const static int in_buffer_max = 1024 * 10;
	uint8_t          in_buffer[in_buffer_max];
	int              in_buffer_used;

	//queue outgoing data
	const static int out_buffer_max = 1024 * 16;
	uint8_t          out_buffer[out_buffer_max];
	int              out_buffer_used;

	//next expected command
	int  command_length;
	int  command_type;
	int  command;

	Client():
		id(-1),
		disconnecting(false),
		socket(0),
		in_buffer_used(0),
		command_length(0),
		command_type(0) {
	}

	~Client() {
		disconnect();
	}

	int connected() const {
		return socket != 0;
	}

	int send_cmd(int cmd, const uint8_t *data, uint32_t length) {
		uint8_t buffer[5];
		buffer[4] = cmd;
		FCEU_en32lsb(buffer, length);

		int sent = send(buffer, 5);
		if (sent && length) {
			sent = send(data, length);
		}

		return sent;
	}

	int send(const uint8_t *data, int32_t length) {
		if (out_buffer_used + length < out_buffer_max) {
			memcpy(&out_buffer[out_buffer_used], data, length);
			out_buffer_used += length;
			return 1;
		}

		disconnect("send buffer full");
		return 0;
	}

	int send() {
		int sent = socket->send(out_buffer, out_buffer_used);
		if (sent != -1) {
			memmove(out_buffer, &out_buffer[sent], out_buffer_used - sent);
			out_buffer_used -= sent;
			return 1;
		}

		disconnect(socket->error());
		return 0;
	}


	void disconnect(const char *reason = "") {
		FCEUD_ServerLog("Client %d %s disconnecting: %s \n", id, name, reason);
		socket->close();
		delete socket;
		socket = 0;
		disconnecting = true;
		out_buffer_used = 0;
	}

	void reset_buffer(int type, int length) {
		if (length > in_buffer_max) {
			fprintf(stderr, "Invalid buffer length %i %s\n", id, name);
			disconnect();
			return;
		}

		command_type  = type;
		in_buffer_used = 0;
		command_length = length;
	}

	void set_default_name() {
		snprintf(name, NETPLAY_MAX_NAME_LEN, "%s%i", FCEU_DEFAULT_NAME, id);
	}

	const static int serialize_len = 1 + NETPLAY_MAX_NAME_LEN;
	void serialize(uint8_t (*buf)[serialize_len]) {
		uint8_t *sbuf = (uint8_t*)buf;
		sbuf[0] = id;
		memcpy(&sbuf[1], name, NETPLAY_MAX_NAME_LEN);
	}
};


struct Server {
	uint64_t frame_delay;
	uint64_t last_time;
	uint8_t *password;

	uint8_t joybuf[5];  /* 4 player data + 1 command byte */

	Client  clients[4];
	Client  *nes_controllers[4];

	ServerConfig config;


	Server():
		last_time(0),
		password(0) {
			for (int i = 0; i < 4; ++i) {
				nes_controllers[i] = 0;
				clients[i].id = i;
			}
			for (int i = 0; i < 5; ++i) {
				joybuf[i] = 0;
			}
	}

	~Server() {
		if (password) {
			delete [] password;
		}
	}

	int unique_name(Client &c) {
		for (int i = 0; i < 4; ++i) {
			if (&c != &clients[i] && clients[i].connected()) {
				if (0 == strcmp(c.name, clients[i].name)) {
					return 0;
				}
			}
		}
		return 1;
	}

	void send_all(int cmd, uint8_t *data, int len) {
		for (int i=0; i < 4; i++) {
			if (clients[i].connected()) {
				clients[i].send_cmd(cmd, data, len);
			}
		}
	}

	void receive() {
		for (int i = 0; i < 4; ++i) {
			receive(clients[i]);
		}
	}

	void receive(Client &client) {
		if (!client.connected())
			return;

		int length = client.socket->recv(
				&client.in_buffer[client.in_buffer_used],
				client.command_length - client.in_buffer_used);

		if (length == -1) {
			client.disconnect();
			return;
		}

		client.in_buffer_used += length;

		if (client.in_buffer_used == client.command_length) {
			process_command(client);
		}
	}

	void check_for_connections() {
		FCEUD_ServerSocket* client_socket = FCEUD_ServerNewConnections();

		if (client_socket) {
			int i;
			for (i = 0; i < 4; ++i) {
				if (!clients[i].connected()) {
					clients[i].socket = client_socket;
					clients[i].reset_buffer(N_LOGINLEN, 4);
					uint8 buf[1];
					buf[0] = config.frame_divisor;
					clients[i].send(buf, 1);
					return;
				}
			}

			//server full
			//TODO announce
			client_socket->close();
			delete client_socket;
		}

	}

	void process_command(Client &client) {
		switch (client.command_type) {
			case N_LOGINLEN: {
				int length = FCEU_de32lsb(client.in_buffer);
				client.reset_buffer(N_LOGIN, length);
				return;
			}

			case N_LOGIN: {
				const int pass_len = 16;

				if (password) {
					if (memcmp(password, client.in_buffer, pass_len)) {
						client.disconnect();
						//TODO: send bad pass
						return;
					}
				}

				int ignored_bytes = 16 + 16 + 64 + 1;
				const int nick_len = client.command_length - ignored_bytes;

				if (nick_len) {
					int len = MIN(nick_len, NETPLAY_MAX_NAME_LEN - 1);
					memcpy(client.name, &client.in_buffer[ignored_bytes], len);
					client.name[len] = 0;

					if (!unique_name(client)) {
						FCEUD_ServerLog("Name %s already in use.\n", client.name);
						client.set_default_name();
					}
				} else {
					client.set_default_name();
				}

				FCEUD_ServerLog("Client %d joined as %s\n", client.id, client.name);

				{ //Announce new client's presence to everyone
					uint8_t announce_buffer[client.serialize_len];
					client.serialize(&announce_buffer);
					send_all(FCEUNPCMD_NEWCLIENT, announce_buffer, client.serialize_len);

					//Inform new client of existing clients and controllers 
					for (int i=0; i < 4; i++) {
						//clients
						if (clients[i].connected() && clients[i].id != client.id) {
							uint8_t announce_buffer[client.serialize_len];
							clients[i].serialize(&announce_buffer);
							client.send_cmd(FCEUNPCMD_NEWCLIENT, announce_buffer, client.serialize_len);
						}
						//controllers
						if (nes_controllers[i]) {
							uint8 controller_buffer[2];
							controller_buffer[0] = nes_controllers[i]->id;
							controller_buffer[1] = i;
							send_all(FCEUNPCMD_PICKUPCONTROLLER, controller_buffer, 2);
						}
					}
				}

				client.reset_buffer(N_UPDATEDATA, 1);
				return;
			}

			case N_UPDATEDATA: {
				if (client.in_buffer[0] == 0xFF) {
					client.reset_buffer(N_COMMAND, 5);
					return;
				}

				//Update game input state
				int i = 0;
				for (int j = 0; j < 4; ++j) {
					if (nes_controllers[j] == &client) {
						joybuf[j] = client.in_buffer[i++];
					}
				}

				client.reset_buffer(N_UPDATEDATA, 1);
				return;
			}

			case N_COMMAND: {
				int length  = FCEU_de32lsb(client.in_buffer);
				uint8_t cmd = client.in_buffer[4];
				client.reset_buffer(cmd, length);
				//FCEUD_ServerLog("Command 0x%X received\n", cmd);
				return;
			}

			case FCEUNPCMD_TEXT: {
				send_all(client.command_type, client.in_buffer, client.in_buffer_used);
				client.in_buffer[MIN(client.in_buffer_used, client.in_buffer_max - 1)] = '\0';
				fprintf(stderr, "%i %s: %s\n", client.id, client.name, client.in_buffer);
				client.reset_buffer(N_UPDATEDATA, 1);
				return;
			}

			case FCEUNPCMD_ANYCONTROLLER: {
				for (int i = 0; i < 4; ++i) {
					if (!nes_controllers[i]) {
						nes_controllers[i] = &client;

						uint8 buffer[2];
						buffer[0] = client.id;
						buffer[1] = i;
						send_all(FCEUNPCMD_PICKUPCONTROLLER, buffer, 2);

						FCEUD_ServerLog("Client %d %s taking controller %i \n", client.id, client.name, i);
						break;
					}
				}
				client.reset_buffer(N_UPDATEDATA, 1);
				return;
			}

			case FCEUNPCMD_PICKUPCONTROLLER: {
				int controller = client.in_buffer[0];

				if (controller > 3) {
					client.disconnect("Invalid controller id");
					return;
				}

				if (!nes_controllers[controller]) {
					nes_controllers[controller] = &client;

					uint8 buffer[2];
					buffer[0] = client.id;
					buffer[1] = controller;
					send_all(FCEUNPCMD_PICKUPCONTROLLER, buffer, 2);

					FCEUD_ServerLog("Client %d %s taking controller %i \n", client.id, client.name, controller);
				}

				client.reset_buffer(N_UPDATEDATA, 1);
				return;
			}

			case FCEUNPCMD_DROPCONTROLLER: {
				int controller = client.in_buffer[0];

				if (controller > 3) {
					fprintf(stderr, "Invalid controller id\n");
					client.disconnect();
					return;
				}

				if (nes_controllers[controller] == &client) {
					nes_controllers[controller] = 0;

					uint8 buffer[1];
					buffer[0] = controller;
					send_all(FCEUNPCMD_DROPCONTROLLER, buffer, 1);

					FCEUD_ServerLog("Client %d %s dropping controller %i \n", client.id, client.name, controller);
				}

				client.reset_buffer(N_UPDATEDATA, 1);
				return;
			}

			case FCEUNPCMD_POWER:
			case FCEUNPCMD_VSUNICOIN:
			case FCEUNPCMD_VSUNIDIP0:
			case FCEUNPCMD_VSUNIDIP0 + 1:
			case FCEUNPCMD_VSUNIDIP0 + 2:
			case FCEUNPCMD_VSUNIDIP0 + 3:
			case FCEUNPCMD_VSUNIDIP0 + 4:
			case FCEUNPCMD_VSUNIDIP0 + 5:
			case FCEUNPCMD_VSUNIDIP0 + 6:
			case FCEUNPCMD_VSUNIDIP0 + 7:
			case FCEUNPCMD_RESET: {
				send_all(client.command_type, client.in_buffer, client.in_buffer_used);
				client.reset_buffer(N_UPDATEDATA, 1);
				return;
			}

			default: {
				client.disconnect("Invalid command");
				return;
			}
		}
	}

	void send() {
		for (int i = 0; i < 4; ++i) {
			if (clients[i].connected())  {
				clients[i].send(joybuf, 5);
				clients[i].send();
			}
		}
	}

	void set_frame_delay(int frame_divisor) {
		//TODO: figure out how to deal with PAL
		//      838977920;  // ~50.007
		int d = 1008307711; // ~60.1
		frame_delay = 16777216.0 / d * 1000000 * frame_divisor;
	}

	void delay() {
		uint64_t time = FCEUD_ServerGetTicks();
		while (time - last_time < frame_delay) {
			usleep(2000);
			time = FCEUD_ServerGetTicks();
		}

		//if the server falls too far behind, don't speed up.
		if (time - last_time > 4 * frame_delay) {
			last_time = time;
			return;
		}

		last_time += frame_delay;
	}

	void handle_disconnects() {
		for (int c = 0; c < 4; ++c) {
			if (clients[c].disconnecting) {

				clients[c].disconnecting = false;

				for (int n = 0; n < 4; ++n) {
					if (nes_controllers[n] == &clients[c]) {
						nes_controllers[n] = 0;
					}
				}

				uint8 buffer[1];
				buffer[0] = c;
				send_all(FCEUNPCMD_CLIENTDISCONNECT, buffer, 1);
			}
		}
	}
} server;


int ServerStart(const ServerConfig &config) {
	server.config = config;
	server.set_frame_delay(config.frame_divisor);
	return FCEUD_ServerStart(config);
}


void ServerUpdate() {
	server.check_for_connections();
	server.receive();
	server.send();
	server.handle_disconnects();
	server.delay();
}
