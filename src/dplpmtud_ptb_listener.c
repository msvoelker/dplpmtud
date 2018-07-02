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

void dplpmtud_ptb_listener_init(int socket) {
	icmp_socket = socket;
}

void *dplpmtud_ptb_listener(void *arg) {
	LOG_DEBUG_("%s - ptb listener entered", THREAD_NAME);
	struct sockaddr_storage addr;
	socklen_t addrlen;
	char buf[BUFFER_SIZE];
	ssize_t recv_len;
	struct ip *ip_header;
	struct icmp *icmp_header;
	struct icmp6_hdr *icmp6_header;
	
	if (icmp_socket < 0) {
		LOG_ERROR("no icmp socket ready");
		return NULL;
	}
	
	// get src address, bind 
	if (get_src_addr(dplpmtud_socket, &addr, &addrlen) < 0) {
		LOG_ERROR("Could not get source ip address");
		return NULL;
	}
	((struct sockaddr_in *)&addr)->sin_port = 0;
	if (bind(icmp_socket, (const struct sockaddr *)&addr, addrlen) < 0) {
		LOG_PERROR("could not bind icmp socket.");
		return NULL;
	}
	
	while (1) {
		// recv 
		recv_len = recv(icmp_socket, buf, BUFFER_SIZE, 0);
		if (recv_len < 0) {
			LOG_PERROR("error receiving on icmp socket.");
			return NULL;
		}
		
		if (dplpmtud_ip_version == IPv4) {
			if (recv_len < 20) {
				LOG_DEBUG("received less than one IP header");
				continue;
			}
			
			ip_header = (struct ip *) buf;
			if (ntohs(ip_header->ip_len) != recv_len) {
				LOG_DEBUG_("did not receive the full ip packet. %d!=%zd", ntohs(ip_header->ip_len), recv_len);
				continue;
			}
			
			if ((recv_len - ip_header->ip_hl*4) < 8) {
				LOG_DEBUG("icmp packet too small");
				continue;
			}
			
			// check icmp type
			icmp_header = (struct icmp *) (buf + ip_header->ip_hl*4); 
			if ( !(icmp_header->icmp_type == 3 && icmp_header->icmp_code == 4) ) {
				LOG_DEBUG("not an ICMP Destination unreaable - fragmentation needed message");
				continue;
			}
			
			if (verify_ptb4((buf + ip_header->ip_hl*4 + ICMP4_HEADER_SIZE), (recv_len - ip_header->ip_hl*4 - ICMP4_HEADER_SIZE))) {
				signal_probe_return_with_mtu(ntohs(icmp_header->icmp_hun.ih_pmtu.ipm_nextmtu));
			}
		} else if (dplpmtud_ip_version == IPv6) {
			icmp6_header = (struct icmp6_hdr *)buf;
			if ( !(icmp6_header->icmp6_type == ICMP6_PACKET_TOO_BIG) ) {
				LOG_DEBUG("not a Packet Too Big Message");
				continue;
			}
			
			if (verify_ptb6((buf + ICMP6_HEADER_SIZE), (recv_len - ICMP6_HEADER_SIZE))) {
				signal_probe_return_with_mtu(ntohl(icmp6_header->icmp6_mtu));
			}
		}
	}
	
	close(icmp_socket);
	LOG_DEBUG_("%s - leave ptb listener", THREAD_NAME);
	return NULL;
}
