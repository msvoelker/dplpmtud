
#ifndef _DPLPMTUD_PL_H_
#define _DPLPMTUD_PL_H_

enum {
	IPv4,
	IPv6
} dplpmtud_ip_version;

int send_probe(int, uint32_t);
int message_handler(int, void *, size_t, struct sockaddr *, socklen_t);

int verify_ptb4(char *, size_t);
int verify_ptb6(char *, size_t);

int signal_probe_return();
uint32_t get_probe_sequence_number();

#endif /* _DPLPMTUD_UDP_H_ */
