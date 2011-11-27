/****************************************************************************
 * FCEUGX-Net
 * Nintendo Wii/Gamecube Port
 *
 * fceunetwork.cpp
 *
 * Description:
 *
 * History:
 *
 * Name           Date     Description
 * ----------  mm/dd/yyyy  --------------------------------------------------
 * blazzy      11/24/2011  New file
 * midnak      11/25/2011  Netplay:  Get netplay settings from GUI.
 ****************************************************************************/

#include <gctypes.h>
#include <network.h>

#include "fceugx.h"

extern FCEUGI *GameInfo;

static int Socket = -1;

int FCEUD_NetworkConnect() {
	const char *host     = GCSettings.netplayIp;
	const char *name     = GCSettings.netplayName;
	const char *password = GCSettings.netplayPwd;
	const u32 port       = (u32) strtol(GCSettings.netplayPort, NULL, 10);
	uint8 local_players  = 1;


	s32 tcp_socket = net_socket(AF_INET, SOCK_STREAM, 0);
	if (tcp_socket < 0) {
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
		FCEU_DispMessage("Resolving DNS...", 0);
		hostent *he = net_gethostbyname(host);
		if (!he) {
			FCEU_DispMessage("Failed to resolve host.", 0);
			close(tcp_socket);
			return 0;
		}
		memcpy(&address.sin_addr, he->h_addr, he->h_length);
	}


	if (net_connect(tcp_socket, (sockaddr*)&address, sizeof(address))) {
		FCEU_DispMessage("Failed to connect.", 0);
		close(tcp_socket);
		return 0;
	}

	// 4 bytes for length
	// 16 bytes md5 key. Is it a hash of the rom data?
	// 16 bytes md5 password hash.
	// 64 mystery bytes? For future use maybe?
	// 1 byte for local player count
	// remaining bytes for local player name
	u32 sendbuf_len = 4 + 16 + 16 + 64 + 1 + strlen(name);
	uint8 sendbuf[sendbuf_len];
	memset(sendbuf, 0, sendbuf_len);

	uint32 length = htons(sendbuf_len - 4);
	memcpy(sendbuf, &length, sizeof(length));
	memcpy(sendbuf + 4, &GameInfo->MD5.data, 16);

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

	FCEU_DispMessage("Connection established.", 0);
	FCEUI_NetplayStart(local_players, recvbuf[0]);

	Socket = tcp_socket;

	return 1;
}

int FCEUD_SendData(void *data, uint32 len) {
	net_send(Socket, data, len , 0);
	return 1;
}

int FCEUD_RecvData(void *data, uint32 len) {
	int size;

	for (;;) {
		fd_set fds;
		struct timeval tv;

		tv.tv_sec  = 0;
		tv.tv_usec = 100000;

		FD_ZERO(&fds);
		FD_SET(Socket, &fds);

		switch (net_select(Socket + 1, &fds, 0, 0, &tv)) {
			case  0: continue;
			case -1: return 0;
		}

		if (FD_ISSET(Socket,&fds)) {
			size = net_recv(Socket, data, len, 0);

			if (size == int(len)) {
				return 1;
			}

			return 0;
		}
	}
}

void FCEUD_NetworkClose(void) {
	if (Socket != -1) {
		close(Socket);
	}
}

void FCEUD_NetplayText(uint8 *text) {
}
