#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int set_ip_dont_fragment_option(int socket) {
	/*
	int val;
	
	val = 1;
	return setsockopt(socket, IPPROTO_IP, IP_DONTFRAG, &val, sizeof(val));
	*/
	return 0;
}
