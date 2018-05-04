#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include "socket.h"
#include <unistd.h>
#include "logger.h"

// get source address for connected socket.
static int get_src_addr(int connected_socket, struct sockaddr_storage *sa) {
	socklen_t addrlen;
	
	addrlen = sizeof(struct sockaddr_storage);
	memset(sa, 0, sizeof(struct sockaddr_storage));
	if (getsockname(connected_socket, (struct sockaddr *)sa, &addrlen) != 0) {
		LOG_PERROR("get_src_addr - getsockname");
		return -1;
	}
	
	return 0;
}

// returns mtu of local interface with given name
static int get_mtu(int socket, char *interface_name) {
	struct ifreq ifr;
	
	memset(&ifr, 0, sizeof(struct ifreq));
	strcpy(ifr.ifr_name, interface_name);
	if (ioctl(socket, SIOCGIFMTU, &ifr) < 0) {
		perror("ioctl");
	}
	return ifr.ifr_mtu;
}

// returns mtu for local interface with given address.
static int get_interface_mtu(int socket, struct sockaddr *if_addr) {
	struct ifaddrs* interfaces;
	struct ifaddrs* interface;
	int mtu;

	if (getifaddrs(&interfaces) != 0) {
		LOG_PERROR("get_interface_mtu - getifaddrs");
		return -1;
	}

	mtu = 0;
	// look which interface contains the wanted IP.
	// When found, ifa->ifa_name contains the name of the interface (eth0, eth1, ppp0...)
	for (interface = interfaces; interface != NULL; interface = interface->ifa_next) {
		if (interface->ifa_addr == NULL) continue;
		if (interface->ifa_name == NULL) continue;
		if (interface->ifa_addr->sa_family != if_addr->sa_family) continue;
		
		if (if_addr->sa_family == AF_INET) {
			if ( ((struct sockaddr_in *)if_addr)->sin_addr.s_addr == ((struct sockaddr_in *)interface->ifa_addr)->sin_addr.s_addr ) {
				mtu = get_mtu(socket, interface->ifa_name);
				break;
			}
		} else if (if_addr->sa_family == AF_INET6) {
			if ( memcmp( (char *) &(((struct sockaddr_in6 *)if_addr)->sin6_addr.s6_addr),
			             (char *) &(((struct sockaddr_in6 *)interface->ifa_addr)->sin6_addr.s6_addr),
			             sizeof(((struct sockaddr_in6 *)if_addr)->sin6_addr.s6_addr)) ) {
				mtu = get_mtu(socket, interface->ifa_name);
				break;
			}
		}
	}
	
	freeifaddrs(interfaces);
	return mtu;
}

int get_local_if_mtu(int connected_socket) {
	struct sockaddr_storage sa;
	int mtu;
	
	if (get_src_addr(connected_socket, &sa) != 0) {
		return -1;
	}
	mtu = get_interface_mtu(connected_socket, (struct sockaddr *)&sa);
	
	return mtu;
}
