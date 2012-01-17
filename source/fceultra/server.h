#ifndef SERVER_H_
#define SERVER_H_

#define FCEU_PROTOCOL_VERSION     "0.1.0"
#define FCEU_DEFAULT_PORT         4046
#define FCEU_DEFAULT_MAXCLIENTS   4
#define FCEU_DEFAULT_TIMEOUT      5
#define FCEU_DEFAULT_FRAMEDIVISOR 1
#define FCEU_DEFAULT_NAME         "UnnamedClient"

struct ServerConfig {
	int port;
	int max_clients;
	int connect_timeout;
	uint8_t frame_divisor;
	uint8_t *password;

	ServerConfig():
		port(FCEU_DEFAULT_PORT),
		max_clients(FCEU_DEFAULT_MAXCLIENTS),
		connect_timeout(FCEU_DEFAULT_TIMEOUT),
		frame_divisor(FCEU_DEFAULT_FRAMEDIVISOR),
		password(0)
	{}

	~ServerConfig() {
		if (password) {
			delete password;
		}
	}
};

void ServerUpdate();

int ServerStart(const ServerConfig &config);

#endif
