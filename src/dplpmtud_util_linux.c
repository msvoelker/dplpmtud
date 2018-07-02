#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

int set_ip_dont_fragment_option(int socket) {
	int val;
	
	val = IP_PMTUDISC_PROBE;
	return setsockopt(socket, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
}
