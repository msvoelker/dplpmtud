
#ifndef _UDP_PMTUD_DPLPMTUD_MAIN_H_
#define _UDP_PMTUD_DPLPMTUD_MAIN_H_

#include <pthread.h>

enum {
	IPv4,
	IPv6
} dplpmtud_ip_version;

int dplpmtud_socket;
pthread_t main_thread_id;
pthread_t listener_thread_id;
pthread_t prober_thread_id;
pthread_t ptb_listener_thread_id;

#define THREAD_NAME ((pthread_self() == prober_thread_id ? "prober_thread" : (pthread_self() == main_thread_id ? "main_thread" : (pthread_self() == ptb_listener_thread_id ? "ptb_listener_thread" : "caller_thread"))))

#endif /* _UDP_PMTUD_DPLPMTUD_MAIN_H_ */
