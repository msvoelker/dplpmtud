#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "socket.h"
#include "dplpmtud.h"

int main(int argc, char **argv) {
	int socket;
	struct addrinfo addr_hints;
	struct addrinfo addressInfos;
	
	if (argc < 3) {
		fprintf(stderr, "call %s host port\n", argv[0]);
		return 1;
	}
	
	memset(&addr_hints, 0, sizeof(struct addrinfo));
	addr_hints.ai_family = AF_UNSPEC;
	addr_hints.ai_socktype = SOCK_DGRAM;
	
	socket = create_socket(1, argv[1], argv[2], &addr_hints, &addressInfos, sizeof(struct addrinfo));
	if (socket < 0) {
		fprintf(stderr, "could not create socket\n");
		return 1;
	}
	dplpmtud_start(socket, addressInfos.ai_family, 1, 0);
	dplpmtud_wait();
	
	close(socket);
	return 0;
}
