#include <string.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include "socket.h"
#include "dplpmtud.h"

#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main(int argc, char **argv) {
	struct addrinfo addr_hints;
	int socket;
	struct addrinfo addressInfos;
	
	memset(&addr_hints, 0, sizeof(struct addrinfo));
	addr_hints.ai_family = AF_UNSPEC;
	addr_hints.ai_socktype = SOCK_DGRAM; //SOCK_RAW;
	//addr_hints.ai_protocol = IPPROTO_UDP;

	socket = create_socket(0, argv[1], argv[2], &addr_hints, &addressInfos, sizeof(struct addrinfo));
	
	dplpmtud_start(socket, addressInfos.ai_family, 0);
	dplpmtud_wait();
	
	close(socket);
	printf("done\n");
}
