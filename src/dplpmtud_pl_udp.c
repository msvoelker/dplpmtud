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
#include "dplpmtud_util.h"

#include <netinet/ip.h>
#include <netinet/ip6.h>

#define IPv4_HEADER_SIZE 20
// TODO assuming a fix IPv6 header size OK?
#define IPv6_HEADER_SIZE 40
#define IP_HEADER_SIZE ((dplpmtud_ip_version == IPv4) ? IPv4_HEADER_SIZE : IPv6_HEADER_SIZE)
#define UDP_HEADER_SIZE 8

#define FLAG_IF_MTU 1

struct udp_heartbeat_header {
	uint8_t type;
	uint8_t flags;
	uint16_t length;
	uint32_t token;
	uint32_t seq_no;
};

struct udp_heartbeat_packet {
	struct udp_heartbeat_header header;
	uint32_t data;
};

static uint32_t token = 4711;

// message specific 
int dplpmtud_send_probe(int socket, uint32_t probe_size, int flags) {
	LOG_DEBUG("dplpmtud_send_probe entered");
	
	char *udp_payload;
	struct udp_heartbeat_header *heartbeat_request;
	size_t udp_payload_size;
	ssize_t send_return;
	
	if (probe_size < (IP_HEADER_SIZE + UDP_HEADER_SIZE + sizeof(struct udp_heartbeat_header))) {
		udp_payload_size = sizeof(struct udp_heartbeat_header);
	} else {
		udp_payload_size = probe_size - IP_HEADER_SIZE - UDP_HEADER_SIZE;
	}
	udp_payload = malloc(udp_payload_size);
	memset(udp_payload, 0, udp_payload_size);
	
	heartbeat_request = (struct udp_heartbeat_header *)udp_payload;
	heartbeat_request->type = 4;
	heartbeat_request->flags = flags;
	heartbeat_request->length = htons(12);
	heartbeat_request->seq_no = get_probe_sequence_number();
	heartbeat_request->token = token;
	
	send_return = send(socket, (const void *) udp_payload, udp_payload_size, 0);
	free(udp_payload);
	LOG_DEBUG("leave dplpmtud_send_probe");
	return send_return;
}

// message specific 
static int handle_heartbeat_response(struct udp_heartbeat_header *heartbeat_response) {
	LOG_DEBUG("handle_heartbeat_response entered");
	uint32_t seq_no;
	struct udp_heartbeat_packet *heartbeat_response_packet;
	
	seq_no = get_probe_sequence_number();
	LOG_DEBUG_("heartbeat_response->token: %u, token: %u heartbeat_response->seq_no: %u, probe_sequence_number: %u", heartbeat_response->token, token, heartbeat_response->seq_no, seq_no);
	if (heartbeat_response->token == token && heartbeat_response->seq_no == seq_no) {
		if ((heartbeat_response->flags & FLAG_IF_MTU) && ntohs(heartbeat_response->length) == sizeof(struct udp_heartbeat_packet)) {
			heartbeat_response_packet = (struct udp_heartbeat_packet *) heartbeat_response;
			dplpmtud_remote_if_mtu_received(ntohl(heartbeat_response_packet->data));
		}
		dplpmtud_probe_acked();
		LOG_DEBUG("leave handle_heartbeat_response");
		return 1;
	}
	LOG_DEBUG("leave handle_heartbeat_response");
	return 0;
}


// message specific 
static int send_heartbeat_response(struct udp_heartbeat_header *heartbeat_request, int socket, struct sockaddr *to_addr, socklen_t to_addr_len) {
	LOG_DEBUG("send_heartbeat_response entered");
	struct udp_heartbeat_packet heartbeat_respone;
	int local_mtu;
	uint16_t length;
	
	memset(&heartbeat_respone, 0, sizeof(struct udp_heartbeat_packet));
	heartbeat_respone.header.type = 5;
	heartbeat_respone.header.flags = 0;
	length = 12;
	heartbeat_respone.header.seq_no = heartbeat_request->seq_no;
	heartbeat_respone.header.token = heartbeat_request->token;
	
	if (heartbeat_request->flags & FLAG_IF_MTU) {
		local_mtu = get_local_if_mtu(socket);
		LOG_DEBUG_("local_mtu: %d", local_mtu);
		if (local_mtu > 0) {
			heartbeat_respone.header.flags |= FLAG_IF_MTU;
			length = 16;
			heartbeat_respone.data = htonl(local_mtu);
		} else {
			LOG_ERROR("Could not fetch local interface MTU");
		}
	}
	heartbeat_respone.header.length = htons(length);
	
	LOG_DEBUG("leave send_heartbeat_response");
	return sendto(socket, (const void *)&heartbeat_respone, length, 0, to_addr, to_addr_len);
}

// message specific 
int dplpmtud_message_handler(int socket, void *message, size_t message_length, struct sockaddr *from_addr, socklen_t from_addr_len) {
	LOG_DEBUG("dplpmtud_message_handler entered");
	struct udp_heartbeat_header *heartbeat;
	
	if (message_length < 12) {
		LOG_ERROR("message is too small for a heartbeat");
		LOG_DEBUG("leave message_handler");
		return -1;
	}
	heartbeat = (struct udp_heartbeat_header *)message;
	LOG_DEBUG_("heartbeat type: %u, length: %u received", heartbeat->type, ntohs(heartbeat->length));
	
	if (message_length < ntohs(heartbeat->length)) {
		LOG_ERROR_("heartbeat length does not fit. %hu, %zu", ntohs(heartbeat->length), message_length);
		LOG_DEBUG("leave message_handler");
		return -1;
	}
	
	if (heartbeat->type == 4) { /* heartbeat request */
		if (ntohs(heartbeat->length) == 12) {
			LOG_DEBUG("leave message_handler");
			return send_heartbeat_response(heartbeat, socket, from_addr, from_addr_len);
		}
		LOG_ERROR_("heartbeat request is not 12 bytes long. %hu", ntohs(heartbeat->length));
	} else if (heartbeat->type == 5) { /* heartbeat response */
		if ((!(heartbeat->flags & FLAG_IF_MTU) && ntohs(heartbeat->length) == 12)
		  || ((heartbeat->flags & FLAG_IF_MTU) && ntohs(heartbeat->length) == 16)) {
			LOG_DEBUG("leave message_handler");
			return handle_heartbeat_response(heartbeat);
		}
		LOG_ERROR_("heartbeat response with incorrect length. %hu %d", ntohs(heartbeat->length), (heartbeat->flags & FLAG_IF_MTU));
	}
	
	LOG_DEBUG("leave dplpmtud_message_handler");
	return -1;
}

int verify_ptb_token(void *udp_payload, size_t payload_length) {
	struct udp_heartbeat_header *heartbeat_request;
	
	if (payload_length < 8) {
		LOG_DEBUG_("icmp message does not contain token, cannot verify. Only %ld bytes udp payload.", payload_length);
		return 0;
	}
	
	heartbeat_request = (struct udp_heartbeat_header *)udp_payload;
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
