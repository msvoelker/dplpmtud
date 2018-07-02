
#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "logger.h"
#include "dplpmtud_main.h"
#include "dplpmtud_prober.h"
#include "dplpmtud_ptb_listener.h"
#include "dplpmtud_listener.h"

// TODO: increase probe_size better

pthread_t main_thread_id = 0;
pthread_t listener_thread_id;
pthread_t prober_thread_id;
pthread_t ptb_listener_thread_id;
int dplpmtud_socket = 0;

static int dplpmtud_passive_mode;
static int dplpmtud_handle_ptb;

static void *controller(void *arg) {
	LOG_DEBUG_("%s - controller entered", THREAD_NAME);
	struct icmp6_filter icmp6_filt;
	int icmp_socket;
	
	dplpmtud_prober_init();
	if (!dplpmtud_passive_mode) {
		if (dplpmtud_handle_ptb) {
			icmp_socket = -1;
			if (dplpmtud_ip_version == IPv4) {
				icmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); 
			} else {
				icmp_socket = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6); 
				ICMP6_FILTER_SETBLOCKALL(&icmp6_filt);
				ICMP6_FILTER_SETPASS(ICMP6_PACKET_TOO_BIG, &icmp6_filt);
				setsockopt(icmp_socket, IPPROTO_ICMPV6, ICMP6_FILTER, &icmp6_filt, sizeof(icmp6_filt));
			}
			if (icmp_socket < 0) {
				LOG_PERROR("could not create icmp socket.");
			}
			// release super user privilege
			if (setuid(getuid()) != 0) {
				LOG_DEBUG_("%s - could not release super user privilege.", THREAD_NAME);
			}
			dplpmtud_ptb_listener_init(icmp_socket);
			pthread_create(&ptb_listener_thread_id, NULL, dplpmtud_ptb_listener, NULL);
		}
		pthread_create(&prober_thread_id, NULL, dplpmtud_prober, NULL);
	}
	
	listener_thread_id = main_thread_id;
	LOG_DEBUG_("%s - leave controller", THREAD_NAME);
	return dplpmtud_listener(NULL);
}

pthread_t dplpmtud_start(int socket, int address_family, int passive_mode, int handle_ptb) {
	LOG_DEBUG_("%s - dplpmtud_start entered", THREAD_NAME);
	if (main_thread_id != 0) {
		// dplpmtud thread already started
		LOG_INFO_("%s - dplpmtud thread already started", THREAD_NAME);
		LOG_DEBUG_("%s - leave dplpmtud_start", THREAD_NAME);
		return 0;
	}
	if (address_family == AF_INET) {
		dplpmtud_ip_version = IPv4;
	} else if (address_family == AF_INET6) {
		dplpmtud_ip_version = IPv6;
	} else {
		LOG_ERROR_("%s - unknown address family", THREAD_NAME);
		LOG_DEBUG_("%s - leave dplpmtud_start", THREAD_NAME);
		return 0;
	}
	dplpmtud_socket = socket;
	
	dplpmtud_passive_mode = passive_mode;
	if (passive_mode) {
		dplpmtud_handle_ptb = 0;
	} else {
		dplpmtud_handle_ptb = handle_ptb;
	}
	
	pthread_create(&main_thread_id, NULL, controller, NULL);
	LOG_DEBUG_("%s - leave dplpmtud_start", THREAD_NAME);
	return main_thread_id;
}

void dplpmtud_wait() {
	LOG_DEBUG_("%s - dplpmtud_wait entered", THREAD_NAME);
	if (main_thread_id != 0) {
		pthread_join(main_thread_id, NULL);
	}
	LOG_DEBUG_("%s - leave dplpmtud_wait", THREAD_NAME);
}
