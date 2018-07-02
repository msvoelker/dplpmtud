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
	int socketDesc;

	char ip_address[INET6_ADDRSTRLEN];
	struct sockaddr_in *ipv4addr;
	struct sockaddr_in6 *ipv6addr;
	int ret;

	if ((ret = getaddrinfo(hostname, service, hints, &addressInfos)) != 0) {
		perror("getaddrinfo");
		if (getaddrinfo("localhost", service, hints, &addressInfos) != 0) perror("getaddrinfo2");
		return -1;
	}

	socketDesc = -1;
	//printf( "Rechner %s (%s) ...\n", hostname, ai->ai_canonname );
	for (addressInfo = addressInfos; addressInfo != NULL; addressInfo = addressInfo->ai_next) {
		fprintf(stderr, "found address info\n");

		fprintf(stderr, "family: %d (%d=AF_INET, %d=AF_INET6\n", addressInfo->ai_family, AF_INET, AF_INET6);
		fprintf(stderr, "type: %d (%d=SOCK_STREAM)\n", addressInfo->ai_socktype, SOCK_STREAM);
		fprintf(stderr, "protocol: %d (%d=IPPROTO_SCTP)\n", addressInfo->ai_protocol, IPPROTO_SCTP);
		if (addressInfo->ai_family == AF_INET) {
			ipv4addr = (struct sockaddr_in *)addressInfo->ai_addr;
			inet_ntop(addressInfo->ai_family, &ipv4addr->sin_addr, ip_address, INET6_ADDRSTRLEN);
		} else {
			ipv6addr = (struct sockaddr_in6 *)addressInfo->ai_addr;
			inet_ntop(addressInfo->ai_family, &ipv6addr->sin6_addr, ip_address, INET6_ADDRSTRLEN);
		}
		fprintf(stderr, "ip: %s, length: %lu (%lu, %u)\n", ip_address, sizeof(*addressInfo->ai_addr), sizeof(struct sockaddr_in6), addressInfo->ai_addrlen);

		/* try to create a socket */
		if ( (socketDesc = socket(addressInfo->ai_family, addressInfo->ai_socktype, addressInfo->ai_protocol)) < 0 ) {
			perror("socket");
			continue;
		}

		/* try to connect/bind */
		if ( (is_server ?
			bind(socketDesc, addressInfo->ai_addr, addressInfo->ai_addrlen) :
			connect(socketDesc, addressInfo->ai_addr, addressInfo->ai_addrlen)
		//if ( !is_server && (connect(socketDesc, addressInfo->ai_addr, addressInfo->ai_addrlen)
		) < 0 ) {
			perror((is_server ? "bind" : "connect"));
			close(socketDesc);
			socketDesc = -1;
			continue;
		}

		fprintf(stderr, "success\n");
		if (peerAddress != NULL) {
			memcpy(peerAddress, addressInfo, peerAddressLength);
		}
		break;
	}

	freeaddrinfo(addressInfos);
	return socketDesc;
}
