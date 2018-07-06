#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include "logger.h"
#include "dplpmtud_pl.h"
#include "dplpmtud_util.h"
#include "dplpmtud_prober.h"
#include "dplpmtud_main.h"

#define ICMP4_HEADER_SIZE 8
#define ICMP6_HEADER_SIZE 8

#define BUFFER_SIZE (1<<16)

static int icmp_socket;

int dplpmtud_ptb_handler_init(int dplpmtud_socket) {
	struct sockaddr_storage addr;
	socklen_t addrlen;
	struct icmp6_filter icmp6_filt;
	
	icmp_socket = -1;
	if (dplpmtud_ip_version == IPv4) {
		icmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); 
	} else {
		icmp_socket = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6); 
	}
	if (icmp_socket < 0) {
		LOG_PERROR("could not create icmp socket.");
		return icmp_socket;
	}
	
	if (dplpmtud_ip_version == IPv6) {
		ICMP6_FILTER_SETBLOCKALL(&icmp6_filt);
		ICMP6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &icmp6_filt);
		setsockopt(icmp_socket, IPPROTO_ICMPV6, ICMP6_FILTER, &icmp6_filt, sizeof(icmp6_filt));
	}
	
	// get src address, bind 
	if (get_src_addr(dplpmtud_socket, &addr, &addrlen) < 0) {
		LOG_ERROR("Could not get source ip address");
		return -1;
	}
	((struct sockaddr_in *)&addr)->sin_port = 0;
	if (bind(icmp_socket, (const struct sockaddr *)&addr, addrlen) < 0) {
		LOG_PERROR("could not bind icmp socket.");
		return -1;
	}
	return icmp_socket;
}

static void ptb4_handler(char *buf, ssize_t recv_len) {
	LOG_DEBUG("ptb4_handler entered");
	struct ip *ip_header;
	struct icmp *icmp_header;
	
	if (recv_len < 20) {
		LOG_DEBUG("received less than one IP header");
		return;
	}
	
	ip_header = (struct ip *) buf;
	if (ntohs(ip_header->ip_len) != recv_len) {
		LOG_DEBUG_("did not receive the full ip packet. %d!=%zd", ntohs(ip_header->ip_len), recv_len);
		return;
	}
	
	if ((recv_len - ip_header->ip_hl*4) < 8) {
		LOG_DEBUG("icmp packet too small");
		return;
	}
	
	// check icmp type
	icmp_header = (struct icmp *) (buf + ip_header->ip_hl*4); 
	if ( !(icmp_header->icmp_type == 3 && icmp_header->icmp_code == 4) ) {
		LOG_DEBUG("not an ICMP Destination unreaable - fragmentation needed message");
		return;
	}
	
	if (verify_ptb4((buf + ip_header->ip_hl*4 + ICMP4_HEADER_SIZE), (recv_len - ip_header->ip_hl*4 - ICMP4_HEADER_SIZE))) {
		dplpmtud_ptb_received(ntohs(icmp_header->icmp_hun.ih_pmtu.ipm_nextmtu));
	}
	
	LOG_DEBUG("leave ptb4_handler");
}

static void ptb6_handler(char *buf, ssize_t recv_len) {
	LOG_DEBUG("ptb6_handler entered");
	struct icmp6_hdr *icmp6_header;

	icmp6_header = (struct icmp6_hdr *)buf;
	if ( !(icmp6_header->icmp6_type == ICMP6_PACKET_TOO_BIG) ) {
		LOG_DEBUG("not a Packet Too Big Message");
		return;
	}
	
	if (verify_ptb6((buf + ICMP6_HEADER_SIZE), (recv_len - ICMP6_HEADER_SIZE))) {
		dplpmtud_ptb_received(ntohl(icmp6_header->icmp6_mtu));
	}
	
	LOG_DEBUG("leave ptb6_handler");
}

void dplpmtud_icmp_socket_readable(void *arg) {
	ssize_t recv_len;
	char buf[BUFFER_SIZE];
	
	if (icmp_socket < 0) {
		LOG_ERROR("no icmp socket");
		return;
	}
	
	recv_len = recv(icmp_socket, buf, BUFFER_SIZE, 0);
	if (recv_len < 0) {
		LOG_PERROR("error receiving on icmp socket.");
		return;
	}
	
	if (dplpmtud_ip_version == IPv4) {
		ptb4_handler(buf, recv_len);
	} else if (dplpmtud_ip_version == IPv6) {
		ptb6_handler(buf, recv_len);
	}
}
