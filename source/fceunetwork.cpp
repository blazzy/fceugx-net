/****************************************************************************
 * FCEUGX-Net
 * Nintendo Wii/Gamecube Port
 *
 * fceunetwork.cpp
 *
 * Description:
 *
 * TODO:
 *   1.  Implement FCEUD_TellServerToggleReady() and FCEUD_SendPlayerListToClients()
 *   2.  Becoming aware of a dropped client and updating the player list in real time
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * blazzy      11/24/2011  New file
 * midnak      11/25/2011  Netplay:  Get netplay settings from GUI.
 * midnak      12/02/2011  Netplay:  Added FCEUD_TellServerToggleReady(),
 *                         FCEUD_SendPlayerListToClients()
 ****************************************************************************/

#include <gctypes.h>
#include <network.h>
#include <fcntl.h>
#include <errno.h>

#include <ogc/lwp_watchdog.h>

#include "fceugx.h"
#include "fceultra/utils/endian.h"
#include "gui/gui_playerlist.h"
#include "fceultra/server.h"

extern GuiPlayerList *playerList;

static int client_socket = -1;
static int server_socket = -1;

static int poll_one(int socket, int timeout, int event);

#define IOS_O_NONBLOCK  0x04

#define CONNECT_TIMEOUT 4000//ms
#define RECV_TIMEOUT    4000//ms

int skipgfx;

int FCEUD_NetworkConnect() {
	const char *host     = GCSettings.netplayIp;
	const char *name     = GCSettings.netplayNameX;  // TODO:  This function must be able to send more than one name, since we can have more than one player per physical machine
	const char *password = GCSettings.netplayPwd;
	const u32 port       = (u32) strtol(GCSettings.netplayPort, NULL, 10);
	uint8 local_players  = 1;


	s32 tcp_socket = net_socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_socket < 0) {
		// TODO:  Remove this message once we are GUI-driven.  Return a unique
		// value if you want to communicate a specific message.
		FCEU_DispMessage("Failed to create socket.", 0);
		return 0;
	}

	//Disable Nagle algorithm
	//TODO: verify that this actually happens
	u32 tcpopt = 1;
	net_setsockopt(tcp_socket, IPPROTO_TCP, TCP_NODELAY, &tcpopt, sizeof(u32));

	sockaddr_in address;
	address.sin_family      = AF_INET;
	address.sin_port        = htons(port);
	address.sin_addr.s_addr = inet_addr(host);
	
	//A standard gethostbyname implementation would have made this unnecessary
	if (address.sin_addr.s_addr == INADDR_BROADCAST) {
		// TODO:  Remove this message (maybe this can be routed to
		// some debug handler)
		FCEU_DispMessage("Resolving DNS...", 0);
		hostent *he = net_gethostbyname(host);
		if (!he) {
			// TODO:  Remove this message once we are GUI-driven.  Return a unique
			// value if you want to communicate a specific message.
			FCEU_DispMessage("Failed to resolve host.", 0);
			close(tcp_socket);
			return 0;
		}
		memcpy(&address.sin_addr, he->h_addr, he->h_length);
	}

	//Disable blocking so we can have the connect-call timeout
	int flags = net_fcntl(tcp_socket, F_GETFL, 0);
	net_fcntl(tcp_socket, F_SETFL, flags | IOS_O_NONBLOCK);

	u64 start_time = gettime();
	while (-EISCONN != net_connect(tcp_socket, (sockaddr*)&address, sizeof(address))) {
		usleep(1000);
		if ( diff_msec(start_time, gettime()) > CONNECT_TIMEOUT ) {
			// TODO:  Remove this message once we are GUI-driven.  Return a unique
			// value if you want to communicate a specific message.
			FCEU_DispMessage("Failed to connect.", 0);
			close(tcp_socket);
			return 0;
		}
	}

	//reenable blocking
	net_fcntl(tcp_socket, F_SETFL, flags);

	// 4 bytes for length
	// 16 bytes md5 key. Is it a hash of the rom data?
	// 16 bytes md5 password hash.
	// 64 mystery bytes? For future use maybe?
	// 1 byte for local player count
	// remaining bytes for local player name
	u32 sendbuf_len = 4 + 16 + 16 + 64 + 1 + strlen(name);
	uint8 sendbuf[sendbuf_len];
	memset(sendbuf, 0, sendbuf_len);

	FCEU_en32lsb(sendbuf, sendbuf_len - 4);
	//memcpy(sendbuf + 4, &GameInfo->MD5.data, 16);
	memset(sendbuf + 4, 0, 16);

	int password_len = strlen(password);
	if (password_len) {
		md5_context md5;
		uint8 md5out[16];

		md5_starts(&md5);
		md5_update(&md5, (uint8*)password, password_len);
		md5_finish(&md5, md5out);
		memcpy(sendbuf + 4 + 16, md5out, 16);
	}

	sendbuf[4 + 16 + 16 + 64] = (uint8)local_players;
	memcpy(sendbuf + 4 + 16 + 16 + 64 + 1, name, strlen(name));

	net_send(tcp_socket, sendbuf, sendbuf_len, 0);

	uint8 recvbuf[1];

	//TODO check if this byte is received
	net_recv(tcp_socket, recvbuf, 1, 0);

	// TODO:  Remove this message once we are GUI-driven.
	FCEU_DispMessage("Connection established.", 0);
	FCEUI_NetplayStart(local_players, recvbuf[0]);

	client_socket = tcp_socket;

	return 1;
}

int FCEUD_SendData(void *data, uint32 len) {
	net_send(client_socket, data, len , 0);
	return 1;
}

//Run poll a single socket for inbound data
static int poll_one(int socket, int timeout, int event) {
	pollsd sd;
	sd.socket = socket;	
	sd.events = event;

	return net_poll(&sd, 1, timeout);
}

int FCEUD_RecvData(void *data, uint32 len) {
	skipgfx = 0;
	if (1 != poll_one(client_socket, RECV_TIMEOUT, POLLIN)) {
		return 0;
	}

	int size = net_recv(client_socket, data, len, 0);

	if (size == int(len)) {
		if (poll_one(client_socket, 0, POLLIN)) {
			skipgfx = 1;
		}
		return 1;
	}

	return 0;
}

static void UpdatePlayerList();

static int gx_controller = -1;

void FCEUGX_NetplayToggleReady() {
	if (gx_controller == -1) {
		FCEUNET_SendCommand(FCEUNPCMD_ANYCONTROLLER, 0);
	} else {
		for (int i = 0; i < 4; ++i) {
			if (NetplayThisClient == NetplayControllers[i]) {
				char buf[1];
				buf[0] = i;

				FCEUNET_SendCommand(FCEUNPCMD_DROPCONTROLLER, 1);
				if (!FCEUD_SendData(buf, 1)) {
					NetError("Network Error: Ready send failed");
				}
			}
		}
	}
}


void FCEUD_NetworkClose(void) {
	if (client_socket != -1) {
		net_close(client_socket);
	}
	client_socket = -1;

	FCEUI_NetplayStop();
	gx_controller = -1;
}


void FCEUD_NetplayClient(uint8 id, uint8 *name) {
	UpdatePlayerList();
}


void FCEUD_NetplayClientDisconnect(uint8 id) {
	UpdatePlayerList();
}


void FCEUD_NetplayPickupController(uint8 id, uint8 controller) {
	if (&NetplayClients[id] == NetplayThisClient) {
		gx_controller = controller;
	}
	UpdatePlayerList();
}


void FCEUD_NetplayDropController(uint8 controller) {
	if (controller == gx_controller) {
		gx_controller = -1;
	}
	UpdatePlayerList();
}


void FCEUD_NetplayText(uint8 *text) {
}


static void UpdatePlayerList() {
	struct {
		const char *name;
		int controller;
	} players[4] = {{0,0},{0,0},{0,0},{0,0}};

	bool listed[4]   = {0,0,0,0};

	for (int c = 0; c < 4; ++c) {
		if (NetplayClients[c].connected) {
			for (int n = 0; n < 4; ++n) {
				if (&NetplayClients[c] == NetplayControllers[n]) {
					players[n].name       = NetplayClients[c].name;
					players[n].controller = n + 1;
					listed[c] = true;
				}
			}
		}
	}

	for (int c = 0; c < 4; ++c) {
		if (NetplayClients[c].connected) {
			if (!listed[c]) {
				for (int p = 0; p < 4; ++p) {
					if (!players[p].name) {
						players[p].name       = NetplayClients[c].name;
						players[p].controller = 0;
						break;
					}
				}
			}
		}
	}

	char list[(NETPLAY_MAX_NAME_LEN + 3) * 4 + 1];
	int offset = 0;

	for (int i = 0; i < 4; ++i) {
		if (players[i].name) {
			sprintf(&list[offset], "%-*s:%i|", NETPLAY_MAX_NAME_LEN -1, players[i].name, players[i].controller);
			offset += NETPLAY_MAX_NAME_LEN + 2;
		}
	}

	playerList->BuildPlayerList(list);
}


uint64 FCEUD_ServerGetTicks() {
	return ticks_to_microsecs(gettime());
}


int FCEUD_ServerStart(const ServerConfig &config) {
	server_socket = net_socket(AF_INET, SOCK_STREAM, 0);

	int sockopt = 1;
	//Allow socket address to be reused in case it wasn't cleaned up correctly
	if (net_setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof(int)))
		FCEUD_ServerLog("SO_REUSEADDR failed: %s\n", strerror(errno));

	//This doesn't work. Is it even relevant on the wii?
	//Disable nagle algorithm
	//if (net_setsockopt(server_socket, IPPROTO_TCP, TCP_NODELAY, &sockopt, sizeof(int)))
	//	FCEUD_ServerLog("TCP_NODELAY failed: %s\n", strerror(errno));

	//The socket should not block
	int flags = net_fcntl(server_socket, F_GETFL, 0);
	net_fcntl(server_socket, F_SETFL, flags | IOS_O_NONBLOCK);

	sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port        = htons(config.port);

	FCEUD_ServerLog("Binding to port %d...\n", config.port);

	if (net_bind(server_socket, (struct sockaddr *)&addr, sizeof(addr))) {
		FCEUD_ServerLog("bind failed: %s\n", strerror(errno));
		return 0;
	}

	FCEUD_ServerLog("Listening on socket...\n");

	if (net_listen(server_socket, 4)) {
		FCEUD_ServerLog("listen failed: %s\n", strerror(errno));
		return 0;
	}

	return 1;
}


struct Socket: FCEUD_ServerSocket {
	int socket;
	const static int error_max = 100;
	char error_text[error_max];

	Socket(): socket(-1) {
	  error_text[0] = 0;
	}
	Socket(int socket_): socket(socket_) {}

	int send(const uint8 *buffer, int length) {
		int sent = net_send(socket, buffer, length, 0);
		if (sent > -1) {
			return sent;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;
		}

		snprintf(error_text, error_max, "send: %s (%i)\n", strerror(errno), errno);
		close();
		return -1;
	}

	int recv(uint8 *buffer, int expected_length) {
		int length = net_recv(socket, buffer, expected_length, 0);

		if (length == 0 && expected_length) {
			close();
			snprintf(error_text, error_max, "recv: %d\n", length);
			return -1;
		}

		if (length < 0) {
			if (length == -EAGAIN) {
				return 0;
			}

			close();
			snprintf(error_text, error_max, "recv: %s %d\n", strerror(-length), -length);
			return -1;
		}

		return length;
	}

	bool connected() {
		return socket != -1;
	}

	void close() {
		if (socket != -1) {
			net_close(socket);
			socket  = -1;
		}
	}

	char *error() {
		return error_text;
	}
};


FCEUD_ServerSocket* FCEUD_ServerNewConnections() {
	sockaddr_in addr;
	socklen_t len = sizeof(addr);

	int client_socket = net_accept(server_socket, (struct sockaddr *)&addr, &len);
	if (client_socket < 0) {
		return 0;
	}

	int flags = net_fcntl(client_socket, F_GETFL, 0);
	net_fcntl(client_socket, F_SETFL, flags | IOS_O_NONBLOCK);

	FCEUD_ServerLog("Connection from %s\n", inet_ntoa(addr.sin_addr));

	return new Socket(client_socket);
}

extern GuiChatWindow * chatWindow;

void FCEUD_ServerLog(const char *error, ...) {
	if (chatWindow) {
		va_list arg;

		va_start(arg, error);
			int length = vsnprintf(0, 0, error, arg);
			char *str = new char[length + 1];
		va_end(arg);

		va_start(arg, error);
			vsprintf(str, error, arg);
		va_end(arg);

		chatWindow->WriteLn(str);

		delete [] str;
	}
}
