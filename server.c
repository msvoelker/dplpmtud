#include "dplpmtud.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include "socket.h"
#include "dplpmtud.h"

struct udp_heartbeat {
	uint8_t type;
	uint8_t flags;
	uint16_t length;
	uint32_t seq_no;
	uint32_t token;
};

#define BUFFER_SIZE (1<<16)

static int send_heartbeat_response(int socket, struct udp_heartbeat *heartbeat_request, struct sockaddr *to_addr, socklen_t to_addr_len) {
	struct udp_heartbeat heartbeat_respone;
	
	memset(&heartbeat_respone, 0, sizeof(struct udp_heartbeat));
	heartbeat_respone.type = 5;
	heartbeat_respone.length = htons(12);
	heartbeat_respone.seq_no = heartbeat_request->seq_no;
	heartbeat_respone.token = heartbeat_request->token;
	
	return sendto(socket, (const void *)&heartbeat_respone, sizeof(struct udp_heartbeat), 0, (struct sockaddr *)to_addr, to_addr_len);
}

static int handle_message(int socket, void *message, ssize_t message_length, struct sockaddr *from_addr, socklen_t from_addr_len) {
	struct udp_heartbeat *heartbeat_request;
	
	if (message_length < 12) {
		fprintf(stderr, "message too short\n");
		return -1;
	}
	
	heartbeat_request = (struct udp_heartbeat *)message;
	printf("type: %u, length: %u\n", heartbeat_request->type, ntohs(heartbeat_request->length));
	if (heartbeat_request->type == 4 && heartbeat_request->length == htons(12)) {
		// valid heartbeat request
		return send_heartbeat_response(socket, heartbeat_request, from_addr, from_addr_len);
	}
	
	return -1;
}

int main(int argc, char **argv) {
	struct addrinfo addr_hints;
	//struct sockaddr_storage client_addr;
	//socklen_t addr_len;
	//ssize_t len;
	//char buf[BUFFER_SIZE];
	int socket;
	struct addrinfo addressInfos;
	
	//printf("sizeof(struct udp_echo_option): %zd\n", sizeof(struct udp_echo_option));
	//exit(1);
	
	memset(&addr_hints, 0, sizeof(struct addrinfo));
	addr_hints.ai_family = AF_UNSPEC;
	addr_hints.ai_socktype = SOCK_DGRAM; //SOCK_RAW;
	//addr_hints.ai_protocol = IPPROTO_UDP;
	
	socket = create_socket(1, argv[1], argv[2], &addr_hints, &addressInfos, sizeof(struct addrinfo));
	dplpmtud_start(socket, addressInfos.ai_family, 1, 0);
	dplpmtud_wait();
	return 0;
}
