#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/time.h>
#include "logger.h"
#include "dplpmtud_main.h"
#include "dplpmtud_prober.h"

#include <netinet/ip.h>
#include <netinet/ip6.h>

#define IPv4_HEADER_SIZE 20
// TODO assuming a fix IPv6 header size OK?
#define IPv6_HEADER_SIZE 40
#define IP_HEADER_SIZE ((dplpmtud_ip_version == IPv4) ? IPv4_HEADER_SIZE : IPv6_HEADER_SIZE)
#define UDP_HEADER_SIZE 8

struct udp_heartbeat {
	uint8_t type;
	uint8_t flags;
	uint16_t length;
	uint32_t token;
	uint32_t seq_no;
};

static uint32_t token = 4711;

// message specific 
int send_probe(int socket, uint32_t probe_size) {
	LOG_DEBUG_("%s - send_probe entered", THREAD_NAME);
	
	char *udp_payload;
	struct udp_heartbeat *heartbeat_request;
	size_t udp_payload_size;
	
	udp_payload_size = probe_size - IP_HEADER_SIZE - UDP_HEADER_SIZE;
	if (udp_payload_size < 0) {
		udp_payload_size = 0;
	}
	udp_payload = malloc(udp_payload_size);
	memset(udp_payload, 0, udp_payload_size);
	
	heartbeat_request = (struct udp_heartbeat *)udp_payload;
	heartbeat_request->type = 4;
	heartbeat_request->length = htons(12);
	heartbeat_request->seq_no = get_probe_sequence_number();
	heartbeat_request->token = token;
	
	LOG_DEBUG_("%s - leave send_probe", THREAD_NAME);
	return send(socket, (const void *) udp_payload, udp_payload_size, 0);
}

// message specific 
static int handle_heartbeat_response(struct udp_heartbeat *heartbeat_response) {
	uint32_t seq_no;
	
	seq_no = get_probe_sequence_number();
	LOG_DEBUG_("%s - handle_heartbeat_response entered", THREAD_NAME);
	LOG_DEBUG_("%s - heartbeat_response->token: %u, token: %u heartbeat_response->seq_no: %u, probe_sequence_number: %u", THREAD_NAME, heartbeat_response->token, token, heartbeat_response->seq_no, seq_no);
	if (heartbeat_response->token == token && heartbeat_response->seq_no == seq_no) {
		signal_probe_return();
		LOG_DEBUG_("%s - leave handle_heartbeat_response", THREAD_NAME);
		return 1;
	}
	LOG_DEBUG_("%s - leave handle_heartbeat_response", THREAD_NAME);
	return 0;
}


// message specific 
static int send_heartbeat_response(struct udp_heartbeat *heartbeat_request, int socket, struct sockaddr *to_addr, socklen_t to_addr_len) {
	LOG_DEBUG_("%s - send_heartbeat_response entered", THREAD_NAME);
	struct udp_heartbeat heartbeat_respone;
	
	memset(&heartbeat_respone, 0, sizeof(struct udp_heartbeat));
	heartbeat_respone.type = 5;
	heartbeat_respone.length = htons(12);
	heartbeat_respone.seq_no = heartbeat_request->seq_no;
	heartbeat_respone.token = heartbeat_request->token;
	
	LOG_DEBUG_("%s - leave send_heartbeat_response", THREAD_NAME);
	return sendto(socket, (const void *)&heartbeat_respone, sizeof(struct udp_heartbeat), 0, to_addr, to_addr_len);
}

// message specific 
int message_handler(int socket, void *message, size_t message_length, struct sockaddr *from_addr, socklen_t from_addr_len) {
	LOG_DEBUG_("%s - message_handler entered", THREAD_NAME);
	struct udp_heartbeat *heartbeat;
	
	if (message_length < 12) {
		LOG_ERROR_("%s - message is too small for a heartbeat", THREAD_NAME);
		LOG_DEBUG_("%s - leave message_handler", THREAD_NAME);
		return -1;
	}
	heartbeat = (struct udp_heartbeat *)message;
	
	if (ntohs(heartbeat->length) != 12) {
		LOG_ERROR_("%s - heartbeat length is not 12 byte. %hu", THREAD_NAME, heartbeat->length);
		LOG_DEBUG_("%s - leave message_handler", THREAD_NAME);
		return -1;
	}
	
	LOG_DEBUG_("%s - heartbeat type: %u, length: %u received", THREAD_NAME, heartbeat->type, ntohs(heartbeat->length));
	if (heartbeat->type == 4) {
		// heartbeat request
		LOG_DEBUG_("%s - leave message_handler", THREAD_NAME);
		return send_heartbeat_response(heartbeat, socket, from_addr, from_addr_len);
	} else if (heartbeat->type == 5) {
		// valid heartbeat response
		LOG_DEBUG_("%s - leave message_handler", THREAD_NAME);
		return handle_heartbeat_response(heartbeat);
	}
	
	LOG_DEBUG_("%s - leave message_handler", THREAD_NAME);
	return -1;
	
}

int verify_ptb_token(void *udp_payload, size_t payload_length) {
	struct udp_heartbeat *heartbeat_request;
	
	if (payload_length < 8) {
		LOG_DEBUG_("icmp message does not contain token, cannot verify. Only %ld bytes udp payload.", payload_length);
		return 0;
	}
	
	heartbeat_request = (struct udp_heartbeat *)udp_payload;
	if (heartbeat_request->token != token) {
		LOG_DEBUG_("wrong token %u.", heartbeat_request->token);
		return 0;
	}
	
	return 1;
}

int verify_ptb4(char *icmp_payload, size_t payload_length) {
	struct ip *ip_header;
	
	ip_header = (struct ip *) icmp_payload;
	if (payload_length < 1 || ip_header->ip_v != IPVERSION) {
		return 0;
	}
	
	// verify -> compare token
	return verify_ptb_token((icmp_payload + ip_header->ip_hl*4 + UDP_HEADER_SIZE), (payload_length - ip_header->ip_hl*4 - UDP_HEADER_SIZE));
}

int verify_ptb6(char *icmp_payload, size_t payload_length) {
	struct ip6_hdr *ip_header;
	char *next_header;
	u_int8_t next_header_proto;
	size_t length_left;
	struct ip6_ext *ip_extheader;
	
	ip_header = (struct ip6_hdr *) icmp_payload;
	if (payload_length < 8 || (ip_header->ip6_vfc >> 4) != 6) {
		return 0;
	}
	
	length_left = payload_length;
	next_header_proto = ip_header->ip6_nxt;
	next_header = icmp_payload + IPv6_HEADER_SIZE;
	length_left -= IPv6_HEADER_SIZE;
	while (next_header_proto != IPPROTO_UDP && length_left >= 2) {
		ip_extheader = (struct ip6_ext *)next_header;
		next_header_proto = ip_extheader->ip6e_nxt;
		next_header = next_header + ip_extheader->ip6e_len;
		length_left -= ip_extheader->ip6e_len;
	}
	
	return verify_ptb_token((next_header + UDP_HEADER_SIZE), (length_left - UDP_HEADER_SIZE));
}
