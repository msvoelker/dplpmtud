
#ifndef _DPLPMTUD_PL_H_
#define _DPLPMTUD_PL_H_

enum {
	IPv4,
	IPv6
} dplpmtud_ip_version;

uint32_t probe_sequence_number;
pthread_mutex_t heartbeat_response_received_mutex;
pthread_cond_t heartbeat_response_received_cond;
int heartbeat_response_received;

int send_probe(int, uint32_t);
int message_handler(int, void *, size_t, struct sockaddr *, socklen_t);

#endif /* _DPLPMTUD_UDP_H_ */
