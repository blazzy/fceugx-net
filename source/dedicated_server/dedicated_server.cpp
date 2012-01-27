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

#include "driver.h"
#include "netplay.h"
#include "server.h"
#include "utils/md5.h"

static int listen_socket = -1;

#define QUOTE(x) #x
#define STR(x) QUOTE(x)

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


uint64 FCEUD_ServerGetTicks() {
	timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return tp.tv_sec * 1000000 + (tp.tv_nsec / 1000);
}

struct Socket: FCEUD_ServerSocket {
	int socket;

	Socket(): socket(-1) {}
	Socket(int socket_): socket(socket_) {}

	int send(const uint8 *buffer, int length) {
		int sent = ::send(socket, buffer, length, MSG_NOSIGNAL);
		if (sent != -1) {
			return sent;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			return 0;
		}

		fprintf(stderr, "send failed: %s (%i)\n", strerror(errno), errno);
		close();
		return -1;
	}

	int recv(uint8 *buffer, int expected_length) {
		int length = ::recv(socket, buffer, expected_length, MSG_NOSIGNAL);

		if (length == 0 && expected_length) {
			close();
			fprintf(stderr, "recv failed: %s\n", strerror(errno));
			return -1;
		}

		if (length == -1) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return 0;
			}

			close();
			fprintf(stderr, "recv failed: %s\n", strerror(errno));
			return -1;
		}

		return length;
	}

	bool connected() {
		return socket != -1;
	}

	void close() {
		if (socket != -1) {
			::close(socket);
			socket  = -1;
		}
	}
};


//Parse args and initialize config
int load_args(int argc, char **argv, ServerConfig &config) {
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
				printf("FCE Ultra network server version %s\n", FCEU_PROTOCOL_VERSION);
				exit(0);

			case 'p':
				config.port = atoi(optarg);
				continue;
			case 'w':
				struct md5_context md5;
				config.password = new uint8_t[16];
				md5_starts(&md5);
				md5_update(&md5, (uint8_t*)optarg, strlen(optarg));
				md5_finish(&md5, config.password);
				continue;
			case 'm':
				config.max_clients = atoi(optarg);
				continue;
			case 't':
				config.connect_timeout = atoi(optarg);
				continue;
			case 'f':
				config.frame_divisor = atoi(optarg);
				continue;
			case '?':
				return 0;
		}
	} while (opt != -1);

	return 1;
}


int FCEUD_ServerStart(const ServerConfig &config) {
	listen_socket = socket(AF_INET, SOCK_STREAM, 0);

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
	addr.sin_port        = htons(config.port);

	fprintf(stderr, "Binding to port %d...\n", config.port);

	if (bind(listen_socket, (struct sockaddr *)&addr, sizeof(addr))) {
		fprintf(stderr, "bind failed: %s\n", strerror(errno));
		return 0;
	}

	fprintf(stderr, "Listening on socket...\n");

	if (listen(listen_socket, SOMAXCONN)) {
		fprintf(stderr, "listen failed: %s\n", strerror(errno));
		return 0;
	}

	return 1;
}


FCEUD_ServerSocket* FCEUD_ServerNewConnections() {
	sockaddr_in addr;
	socklen_t len = sizeof(addr);

	int client_socket = accept(listen_socket, (struct sockaddr *)&addr, &len);
	if (client_socket == -1) {
		if (errno != EAGAIN && errno != EWOULDBLOCK)
			fprintf(stderr, "accept failed: %s\n", strerror(errno));
		return 0;
	}

	int flags = fcntl(client_socket, F_GETFL, 0);
	fcntl(client_socket, F_SETFL, flags | O_NONBLOCK);

	fprintf(stderr, "Connection from %s\n", inet_ntoa(addr.sin_addr));

	return new Socket(client_socket);
}


int main(int argc, char *argv[]) {
	ServerConfig config;

	if (!load_args(argc, argv, config))
		return -1;

	ServerStart(config);

	while (1) {
		ServerUpdate();
	}
}
