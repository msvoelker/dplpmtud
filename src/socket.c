#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <string.h>

int create_socket(int is_server, char *hostname, char *service, struct addrinfo *hints, struct addrinfo *peerAddress, size_t peerAddressLength) {
	struct addrinfo *addressInfos, *addressInfo;
	int socket_desc;

	if (getaddrinfo(hostname, service, hints, &addressInfos) != 0) {
		perror("getaddrinfo");
		return -1;
	}

	socket_desc = -1;
	for (addressInfo = addressInfos; addressInfo != NULL && socket_desc < 0; addressInfo = addressInfo->ai_next) {
		/* try to create a socket */
		if ( (socket_desc = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol)) < 0 ) {
			perror("socket");
			continue;
		}

		/* try to connect/bind */
		if ( (is_server ?
		  bind(socket_desc, addressInfo->ai_addr, addressInfo->ai_addrlen) :
		  connect(socket_desc, addressInfo->ai_addr, addressInfo->ai_addrlen)) < 0 ) {
			perror((is_server ? "bind" : "connect"));
			close(socket_desc);
			socket_desc = -1;
			continue;
		}

		if (peerAddress != NULL) {
			memcpy(peerAddress, addressInfo, peerAddressLength);
		}
	}

	freeaddrinfo(addressInfos);
	return socket_desc;
}
