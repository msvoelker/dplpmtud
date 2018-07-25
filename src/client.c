#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include "socket.h"
#include "dplpmtud.h"

int main(int argc, char **argv) {
	struct addrinfo addr_hints;
	struct addrinfo addressInfos;
	char *host;
	char *port;
	int socket;
	int handle_ptb;
	
	handle_ptb = 0;
	if (argc == 3) {
		host = argv[1];
		port = argv[2];
	} else if (argc == 4) {
		if (strcmp(argv[1], "--handle-ptb") == 0) {
			handle_ptb = 1;
		} else {
			fprintf(stderr, "unknown option %s\n", argv[1]);
			return 1;
		}
		host = argv[2];
		port = argv[3];
	} else {
		fprintf(stderr, "call %s [--handle-ptb] host port\n", argv[0]);
		return 1;
	}
	
	memset(&addr_hints, 0, sizeof(struct addrinfo));
	addr_hints.ai_family = AF_UNSPEC;
	addr_hints.ai_socktype = SOCK_DGRAM;

	socket = create_socket(0, host, port, &addr_hints, &addressInfos, sizeof(struct addrinfo));
	if (socket < 0) {
		fprintf(stderr, "could not create socket\n");
		return 1;
	}
	dplpmtud_start(socket, addressInfos.ai_family, 1, handle_ptb);
	dplpmtud_wait();
	
	close(socket);
}
