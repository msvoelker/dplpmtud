#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "logger.h"

int set_ip_dont_fragment_option(int socket) {
	/*
	int val;
	
	val = 1;
	return setsockopt(socket, IPPROTO_IP, IP_DONTFRAG, &val, sizeof(val));
	*/
	LOG_ERROR("Cannot set IPv4 Don't Fragment Flag on OSX. Probing on OSX is supported with IPv6 only.");
	return -1;
}
