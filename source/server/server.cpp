/* FCE Ultra Network Play Server
 *
 * Copyright notice for this file:
 *  Copyright (C) 2004 Xodnizel
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "md5.h"
#include "fceultra/netplay.h"

#define QUOTE(x) #x
#define STR(x) QUOTE(x)
#define MIN(a,b) (((a)<(b))?(a):(b))

#define VERSION              "0.1.0"
#define DEFAULT_PORT         4046
#define DEFAULT_MAXCLIENTS   4
#define DEFAULT_TIMEOUT      5
#define DEFAULT_FRAMEDIVISOR 1
#define DEFAULT_NAME         "UnnamedClient"

#define N_LOGINLEN   0x1000
#define N_LOGIN      0x2000
#define N_COMMAND    0x4000
#define N_UPDATEDATA 0x5000

const char *usage =
	"Usage: %s [OPTION]...\n"
	"Begins the FCE Ultra game server with given options.\n"
	"-h, --help          Displays this help message.\n"
	"-v, --version       Displays the version number and quits.\n"
	"-p, --port          Starts server on given port. (default=" STR(DEFAULT_PORT) ")\n"
	"-w, --password      Specifies a password for entry.\n"
	"-m, --maxclients    Specifies the maximum amount of clients allowed\n"
	"                    to access the server. (default=" STR(DEFAULT_MAXCLIENTS) ")\n"
	"-t, --timeout       Specifies the amount of seconds before the server\n"
	"                    times out. (default=" STR(DEFAULT_TIMEOUT) ")\n"
	"-f, --framedivisor  Specifies frame divisor.\n"
	"                    (eg: 60 / framedivisor = updates per second)(default=" STR(DEFAULT_FRAMEDIVISOR) ")\n";


static void en32(uint8_t *buf, uint32_t morp) {
	buf[0]=morp;
	buf[1]=morp>>8;
	buf[2]=morp>>16;
	buf[3]=morp>>24;
}


static uint32_t de32(uint8_t *morp) {
	return morp[0]|(morp[1]<<8)|(morp[2]<<16)|(morp[3]<<24);
}


static uint64_t gettime() {
	timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return tp.tv_sec * 1000000 + (tp.tv_nsec / 1000);
}


struct Client {
	char name[NETPLAY_MAX_NAME_LEN];
	int  id;
	bool disconnecting;

	int  socket;

	//to store incoming data over the socket
	const static int in_buffer_max = 1024 * 10;
	uint8_t          in_buffer[in_buffer_max];
	int              in_buffer_used;

	//to store incoming data over the socket
	const static int out_buffer_max = 1024 * 16;
	uint8_t          out_buffer[out_buffer_max];
	int              out_buffer_used;

	//next expected command
	int  command_length;
	int  command_type;
	int  command;

	Client():
		id(-1),
		socket(-1),
		disconnecting(false),
		in_buffer_used(0),
		command_length(0),
		command_type(0) {
	}

	~Client() {
		disconnect();
	}

	int connected() const {
		return (socket != -1);
	}

	int send_cmd(int cmd, uint8_t *data, uint32_t length) {
		uint8_t buffer[5];
		buffer[4] = cmd;
		en32(buffer, length);

		int sent = send(buffer, 5);
		if (sent && length) {
			sent = send(data, length);
		}

		return sent;
	}

	int send(uint8_t *data, uint32_t length) {
		if (out_buffer_used + length < out_buffer_max) {
			memcpy(&out_buffer[out_buffer_used], data, length);
			out_buffer_used += length;
			return 1;
		}

		fprintf(stderr, "send failed: send buffer full\n");
		disconnect();
		return 0;
	}

	int send() {
		int sent = ::send(socket, out_buffer, out_buffer_used, MSG_NOSIGNAL);
		if (sent == out_buffer_used || errno == EAGAIN || errno == EWOULDBLOCK) {
			memmove(out_buffer, &out_buffer[sent], out_buffer_used - sent);
      out_buffer_used -= sent;
			return 1;
		}

		fprintf(stderr, "send failed: %s (%i)\n", strerror(errno), errno);
		disconnect();
		return 0;
	}

	void disconnect() {
		fprintf(stderr, "Client %d %s disconnecting\n", id, name);
		if (socket != -1) {
			close(socket);

			socket  = -1;
			disconnecting = true;
		}
	}

	void reset_buffer(int type, int length) {
		if (length > in_buffer_max) {
			fprintf(stderr, "Invalid buffer length\n", id, name);
			disconnect();
			return;
		}

		command_type  = type;
		in_buffer_used = 0;
		command_length = length;
	}

	void set_default_name() {
		snprintf(name, NETPLAY_MAX_NAME_LEN, "%s%i", DEFAULT_NAME, id);
	}

	const static int serialize_len = 1 + NETPLAY_MAX_NAME_LEN;
	void serialize(uint8_t (*buf)[serialize_len]) {
		uint8_t *sbuf = (uint8_t*)buf;
		sbuf[0] = id;
		memcpy(&sbuf[1], name, NETPLAY_MAX_NAME_LEN);
	}
};


struct Game {
	Game():
		port(DEFAULT_PORT),
		max_clients(DEFAULT_MAXCLIENTS),
		connect_timeout(DEFAULT_TIMEOUT),
		frame_divisor(DEFAULT_FRAMEDIVISOR),
		password(0),
		last_time(0) {
			for (int i = 0; i < 4; ++i) {
				nes_controllers[i] = 0;
				clients[i].id = i;
			}
			for (int i = 0; i < 5; ++i) {
				joybuf[i] = 0;
			}
	}

	~Game() {
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

	int send_all(int cmd, uint8_t *data, int len) {
		uint8_t buffer[5];
		buffer[4] = cmd;

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
		while (1) {
			if (!client.connected())
				return;

			int length = recv(
					client.socket,
					&client.in_buffer[client.in_buffer_used],
					client.command_length - client.in_buffer_used,
					MSG_NOSIGNAL);

			if (length == 0 && client.command_length) {
				client.disconnect();
				return;
			}

			if (length == -1) {
				if (errno != EAGAIN && errno != EWOULDBLOCK) {
					client.disconnect();
					fprintf(stderr, "recv failed: %s\n", strerror(errno));
				}
				return;
			}

			client.in_buffer_used += length;

			if (client.in_buffer_used == client.command_length) {
				process_command(client);
			}
		}
	}

	void process_command(Client &client) {
		switch (client.command_type) {
			case N_LOGINLEN: {
				int length = de32(client.in_buffer);
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
						fprintf(stderr, "Name %s already in use.\n", client.name);
						client.set_default_name();
					}
				} else {
					client.set_default_name();
				}

				fprintf(stderr, "Client %d joined as %s\n", client.id, client.name);

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
				int length  = de32(client.in_buffer);
				uint8_t cmd = client.in_buffer[4];
				client.reset_buffer(cmd, length);
				fprintf(stderr, "Command 0x%X received\n", cmd);
				return;
			}

			case FCEUNPCMD_TEXT: {
				send_all(client.command_type, client.in_buffer, client.in_buffer_used);
				client.in_buffer[MIN(client.in_buffer_used, client.in_buffer_max - 1)] = '\0';
				fprintf(stderr, "%i %s: %s\n", client.id, client.name, client.in_buffer);
				client.reset_buffer(N_UPDATEDATA, 1);
				return;
			}

			case FCEUNPCMD_PICKUPCONTROLLER: {
				int controller = client.in_buffer[0];

				if (controller > 3) {
					fprintf(stderr, "Invalid controller id\n");
					client.disconnect();
					return;
				}

				if (!nes_controllers[controller]) {
					nes_controllers[controller] = &client;

					uint8 buffer[2];
					buffer[0] = client.id;
					buffer[1] = controller;
					send_all(FCEUNPCMD_PICKUPCONTROLLER, buffer, 2);

					fprintf(stderr, "Client %d %s taking controller %i \n", client.id, client.name, controller);
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

					fprintf(stderr, "Client %d %s dropping controller %i \n", client.id, client.name, controller);
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
				fprintf(stderr, "Invalid command\n");
				client.disconnect();
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
		uint64_t time = gettime();
		while (time - last_time < frame_delay) {
			usleep(2000);
			time = gettime();
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

	int port;
	int max_clients;
	int connect_timeout;
	uint8_t frame_divisor;
	uint64_t frame_delay;
	uint64_t last_time;
	uint8_t *password;

	uint8_t joybuf[5];  /* 4 player data + 1 command byte */

	Client  clients[4];
	Client  *nes_controllers[4];
};


//Parse args and initialize config
int load_args(int argc, char **argv, Game &game) {
	option longopts[] = {
		{ "help"        , 0, 0, 'h'},
		{ "port"        , 1, 0, 'p'},
		{ "password"    , 1, 0, 'w'},
		{ "maxclients"  , 1, 0, 'm'},
		{ "timeout"     , 1, 0, 't'},
		{ "framedivisor", 1, 0, 'f'},
		{ 0             , 0, 0, 0  }
	};

	int opt;
	do {
		opt = getopt_long(argc, argv, "hvp:w:m:t:f:c:", longopts, 0);

		switch (opt) {
			case 'h':
				printf(usage, argv[0]);
				exit(0);
			case 'v':
				printf("FCE Ultra network server version %s\n", VERSION);
				exit(0);

			case 'p':
				game.port = atoi(optarg);
				continue;
			case 'w':
				struct md5_context md5;
				game.password = new uint8_t[16];
				md5_starts(&md5);
				md5_update(&md5, (uint8_t*)optarg, strlen(optarg));
				md5_finish(&md5, game.password);
				continue;
			case 'm':
				game.max_clients = atoi(optarg);
				continue;
			case 't':
				game.connect_timeout = atoi(optarg);
				continue;
			case 'f':
				game.frame_divisor = atoi(optarg);
				continue;
			case '?':
				return 0;
		}
	} while (opt != -1);

	return 1;
}


//Setup and return a socket to listen for connections
int start_listening(const Game &game) {
	int listen_socket = socket(AF_INET, SOCK_STREAM, 0);

	int sockopt = 1;
	//Allow socket address to be reused in case it wasn't cleaned up correctly
	if (setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(int)))
		fprintf(stderr, "SO_REUSEADDR failed: %s\n", strerror(errno));

	//Disable nagle algorithm
	if (setsockopt(listen_socket, IPPROTO_TCP, TCP_NODELAY, &sockopt, sizeof(int)))
		fprintf(stderr, "TCP_NODELAY failed: %s\n", strerror(errno));

	//The socket should not block
	int flags = fcntl(listen_socket, F_GETFL, 0);
	fcntl(listen_socket, F_SETFL, flags | O_NONBLOCK);

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port        = htons(game.port);

	fprintf(stderr, "Binding to port %d...\n", game.port);

	if (bind(listen_socket, (struct sockaddr *)&addr, sizeof(addr))) {
		fprintf(stderr, "bind failed: %s\n", strerror(errno));
		return 0;
	}

	fprintf(stderr, "Listening on socket...\n");

	if (listen(listen_socket, SOMAXCONN)) {
		fprintf(stderr, "listen failed: %s\n", strerror(errno));
		return 0;
	}

	return listen_socket;
}


void check_for_connections(int listen_socket, Game &game) {
	sockaddr_in addr;
	socklen_t len = sizeof(addr);

	int client_socket = accept(listen_socket, (struct sockaddr *)&addr, &len);
	if (client_socket == -1) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			fprintf(stderr, "accept failed: %s\n", strerror(errno));
		return;
	}

	int flags = fcntl(client_socket, F_GETFL, 0);
	fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

	int i;
	for (i = 0; i < 4; ++i) {
		if (!game.clients[i].connected()) {
			game.clients[i].socket = client_socket;
			game.clients[i].reset_buffer(N_LOGINLEN, 4);
			break;
		}
	}

	//server full
	if (i == 4) {
		close(client_socket);
		//TODO announce
		return;
	}

	fprintf(stderr, "Client %d connecting from %s\n", i, inet_ntoa(addr.sin_addr));

	game.clients[i].send(&game.frame_divisor, 1);
}


int main(int argc, char *argv[]) {
	Game game;

	if (!load_args(argc, argv, game))
		return -1;

	int listen_socket = start_listening(game);

	game.set_frame_delay(game.frame_divisor);

	while (1) {
		check_for_connections(listen_socket, game);

		game.receive();
		game.send();
		game.handle_disconnects();

		game.delay();
	}
}
