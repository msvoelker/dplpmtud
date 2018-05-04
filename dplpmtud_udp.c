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
#include "dplpmtud_pl.h"

#define IPv4_HEADER_SIZE 20
// TODO assuming a fix IPv6 header size OK?
#define IPv6_HEADER_SIZE 40
#define IP_HEADER_SIZE ((dplpmtud_ip_version == IPv4) ? IPv4_HEADER_SIZE : IPv6_HEADER_SIZE)
#define UDP_HEADER_SIZE 8

#define THREAD_NAME_SEND "dplpmtud_thread"
#define THREAD_NAME_RECEIVE "listener_thread"

struct udp_heartbeat {
	uint8_t type;
	uint8_t flags;
	uint16_t length;
	uint32_t seq_no;
	uint32_t token;
};

static uint32_t token = 4711;

// message specific 
int send_probe(int socket, uint32_t probe_size) {
	LOG_DEBUG("%s - send_probe entered", THREAD_NAME_SEND);
	//size_t probe_size = 1300;
	
	// assert(probe_size > IPv4_HEADER_SIZE + 6)
	
	char *udp_payload;
	struct udp_heartbeat *heartbeat_request;
	size_t udp_payload_size;
	
	udp_payload_size = probe_size - IP_HEADER_SIZE - UDP_HEADER_SIZE;
	udp_payload = malloc(udp_payload_size);
	memset(udp_payload, 0, udp_payload_size);
	
	heartbeat_request = (struct udp_heartbeat *)udp_payload;
	heartbeat_request->type = 4;
	heartbeat_request->length = htons(12);
	heartbeat_request->seq_no = probe_sequence_number;
	heartbeat_request->token = token;
	
	LOG_DEBUG("%s - leave send_probe", THREAD_NAME_SEND);
	return send(socket, (const void *) udp_payload, udp_payload_size, 0);
}

// message specific 
static int handle_heartbeat_response(struct udp_heartbeat *heartbeat_response) {
	LOG_DEBUG("%s - handle_heartbeat_response entered", THREAD_NAME_RECEIVE);
	LOG_DEBUG("%s - heartbeat_response->token: %u, token: %u heartbeat_response->seq_no: %u, probe_sequence_number: %u", THREAD_NAME_RECEIVE, heartbeat_response->token, token, heartbeat_response->seq_no, probe_sequence_number);
	if (heartbeat_response->token == token && heartbeat_response->seq_no == probe_sequence_number) {
		heartbeat_response_received = 1;
		LOG_DEBUG("%s - heartbeat_response_received = 1", THREAD_NAME_RECEIVE);
		pthread_mutex_lock(&heartbeat_response_received_mutex);
		pthread_cond_signal(&heartbeat_response_received_cond);
		pthread_mutex_unlock(&heartbeat_response_received_mutex);
		LOG_DEBUG("%s - leave handle_heartbeat_response", THREAD_NAME_RECEIVE);
		return 1;
	}
	LOG_DEBUG("%s - leave handle_heartbeat_response", THREAD_NAME_RECEIVE);
	return 0;
}


// message specific 
static int send_heartbeat_response(struct udp_heartbeat *heartbeat_request, int socket, struct sockaddr *to_addr, socklen_t to_addr_len) {
	LOG_DEBUG("%s - send_heartbeat_response entered", THREAD_NAME_RECEIVE);
	struct udp_heartbeat heartbeat_respone;
	
	memset(&heartbeat_respone, 0, sizeof(struct udp_heartbeat));
	heartbeat_respone.type = 5;
	heartbeat_respone.length = htons(12);
	heartbeat_respone.seq_no = heartbeat_request->seq_no;
	heartbeat_respone.token = heartbeat_request->token;
	
	LOG_DEBUG("%s - leave send_heartbeat_response", THREAD_NAME_RECEIVE);
	return sendto(socket, (const void *)&heartbeat_respone, sizeof(struct udp_heartbeat), 0, to_addr, to_addr_len);
}

// message specific 
int message_handler(int socket, void *message, size_t message_length, struct sockaddr *from_addr, socklen_t from_addr_len) {
	LOG_DEBUG("%s - message_handler entered", THREAD_NAME_RECEIVE);
	struct udp_heartbeat *heartbeat;
	
	if (message_length < 12) {
		LOG_ERROR("%s - message is too small for a heartbeat", THREAD_NAME_RECEIVE);
		LOG_DEBUG("%s - leave message_handler", THREAD_NAME_RECEIVE);
		return -1;
	}
	heartbeat = (struct udp_heartbeat *)message;
	
	if (ntohs(heartbeat->length) != 12) {
		LOG_ERROR("%s - heartbeat length is not 12 byte. %hu", THREAD_NAME_RECEIVE, heartbeat->length);
		LOG_DEBUG("%s - leave message_handler", THREAD_NAME_RECEIVE);
		return -1;
	}
	
	LOG_DEBUG("%s - heartbeat type: %u, length: %u received", THREAD_NAME_RECEIVE, heartbeat->type, ntohs(heartbeat->length));
	if (heartbeat->type == 4) {
		// heartbeat request
		LOG_DEBUG("%s - leave message_handler", THREAD_NAME_RECEIVE);
		return send_heartbeat_response(heartbeat, socket, from_addr, from_addr_len);
	} else if (heartbeat->type == 5) {
		// valid heartbeat response
		LOG_DEBUG("%s - leave message_handler", THREAD_NAME_RECEIVE);
		return handle_heartbeat_response(heartbeat);
	}
	
	LOG_DEBUG("%s - leave message_handler", THREAD_NAME_RECEIVE);
	return -1;
	
}
