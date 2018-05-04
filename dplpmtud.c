
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
#include "dplpmtud_states.h"
#include "dplpmtud_pl.h"
#include "dplpmtud_os.h"

// TODO: Handle PTB messages 
// TODO: increase probe_size better

#define BASE_PMTU_IPv6 1280
#define BASE_PMTU_IPv4 1200
#define BASE_PMTU ((dplpmtud_ip_version == IPv4) ? BASE_PMTU_IPv4 : BASE_PMTU_IPv6)
#define MAX_PROBES 10
#define MIN_PMTU_IPv6 1280
#define MIN_PMTU_IPv4 68
#define MIN_PMTU ((dplpmtud_ip_version == IPv4) ? MIN_PMTU_IPv4 : MIN_PMTU_IPv6)

#define PROBE_TIMEOUT 20
#define REACHABILITY_TIMEOUT 100
#define RAISE_TIMEOUT 600

#define BUFFER_SIZE (1<<16)

static uint32_t probe_size;
static uint32_t probed_size_success;
static pthread_t main_thread_id = NULL;
static pthread_t listener_thread_id;
static int dplpmtud_socket = 0;
static int dplpmtud_listener_only;
static uint32_t max_pmtu = 1500;

#define THREAD_NAME ((pthread_self() == listener_thread_id ? "listener_thread" : (pthread_self() == main_thread_id ? "main_thread" : "caller_thread")))

static void increase_probe_size() {
	probe_size += 50;
	if (probe_size > max_pmtu) {
		probe_size = max_pmtu;
	}
}

static int do_probe(int do_increase_probe_size) {
	LOG_DEBUG("%s - do_probe entered", THREAD_NAME);
	uint32_t probe_count;
	struct timespec stop_time;
	struct timeval now;
	
	probe_count = 0;
	
	pthread_mutex_lock(&heartbeat_response_received_mutex);
	LOG_DEBUG("%s - mutex locked", THREAD_NAME);
	while (probe_count < MAX_PROBES) {
		probe_sequence_number++;
		LOG_DEBUG("%s - probe_sequence_number: %u", THREAD_NAME, probe_sequence_number);
		heartbeat_response_received = 0;
		send_probe(dplpmtud_socket, probe_size);
		LOG_DEBUG("%s - cond wait", THREAD_NAME);
		gettimeofday(&now, NULL);
		stop_time.tv_sec = now.tv_sec+PROBE_TIMEOUT;
		stop_time.tv_nsec = now.tv_usec * 1000;
		pthread_cond_timedwait(&heartbeat_response_received_cond, &heartbeat_response_received_mutex, &stop_time); 
		LOG_DEBUG("%s - heartbeat_response_received == %d", THREAD_NAME, heartbeat_response_received);
		if (heartbeat_response_received) {
			if (do_increase_probe_size) {
				probed_size_success = probe_size;
				if (probe_size == max_pmtu) {
					pthread_mutex_unlock(&heartbeat_response_received_mutex);
					LOG_DEBUG("%s - max_pmtu acked", THREAD_NAME);
					LOG_DEBUG("%s - leave do_probe", THREAD_NAME);
					return 1;
				}
				increase_probe_size();
			} else {
				pthread_mutex_unlock(&heartbeat_response_received_mutex);
				LOG_DEBUG("%s - leave do_probe", THREAD_NAME);
				return 1;
			}
		} else { // probe timer expired
			pthread_mutex_unlock(&heartbeat_response_received_mutex);
			LOG_DEBUG("%s - leave do_probe", THREAD_NAME);
			return 0;
		}
	}
	pthread_mutex_unlock(&heartbeat_response_received_mutex);
	LOG_DEBUG("%s - leave do_probe", THREAD_NAME);
	return 0;
}

static int is_raise_timer_expired(time_t raise_timer_start) {
	if ((time(NULL) - raise_timer_start) >= RAISE_TIMEOUT) {
		LOG_DEBUG("%s - RAISE_TIMER expired", THREAD_NAME);
		return 1;
	}
	return 0;
}

state_t run_base_state() {
	LOG_DEBUG("%s - run_base_state entered", THREAD_NAME);
	int probe_success;
	
	probe_size = BASE_PMTU;
	probe_success = do_probe(0);
	if (probe_success) {
		LOG_DEBUG("%s - leave run_base_state", THREAD_NAME);
		return SEARCH;
	}
	LOG_DEBUG("%s - leave run_base_state", THREAD_NAME);
	return ERROR;
}

state_t run_search_state() {
	LOG_DEBUG("%s - run_search_state entered", THREAD_NAME);
	do_probe(1);
	LOG_DEBUG("%s - leave run_search_state", THREAD_NAME);
	return DONE;
}

state_t run_done_state() {
	LOG_DEBUG("%s - run_done_state entered", THREAD_NAME);
	int probe_success;
	time_t raise_timer_start;
	
	probe_size = probed_size_success;
	raise_timer_start = time(NULL);
	probe_success = 1;
	while (probe_success) {
		LOG_DEBUG("%s - sleep for REACHABILITY_TIMEOUT", THREAD_NAME);
		sleep(REACHABILITY_TIMEOUT);
		if (is_raise_timer_expired(raise_timer_start)) break;
		probe_success = do_probe(0);
		if (is_raise_timer_expired(raise_timer_start)) break;
	}
	
	LOG_DEBUG("%s - leave run_done_state", THREAD_NAME);
	return BASE;
}

state_t run_error_state() {
	LOG_DEBUG("%s - run_error_state entered", THREAD_NAME);
	int probe_success;
	
	probe_size = MIN_PMTU;
	probe_success = 0;
	while (!probe_success) {
		probe_success = do_probe(0);
	}
	
	LOG_DEBUG("%s - leave run_error_state", THREAD_NAME);
	return SEARCH;
}

static void *listener(void *arg) {
	LOG_DEBUG("%s - listener entered", THREAD_NAME);
	ssize_t recv_len;
	char buf[BUFFER_SIZE];
	struct sockaddr_storage from_addr;
	socklen_t from_addr_len;
	
	for (;;) {
		from_addr_len = (socklen_t) sizeof(from_addr);
		memset((void *) &from_addr, 0, sizeof(from_addr));
		recv_len = recvfrom(dplpmtud_socket, buf, BUFFER_SIZE, 0, (struct sockaddr *) &from_addr, &from_addr_len);
		message_handler(dplpmtud_socket, buf, recv_len, (struct sockaddr *)&from_addr, from_addr_len);
	}
	
	LOG_DEBUG("%s - leave listener", THREAD_NAME);
	return NULL;
}

static void *controller(void *arg) {
	LOG_DEBUG("%s - controller entered", THREAD_NAME);
	state_t state;
	int mtu;
	
	if (dplpmtud_listener_only) {
		listener_thread_id = main_thread_id;
		return listener(NULL);
	}
	probe_sequence_number = 0;
	mtu = get_local_if_mtu(dplpmtud_socket);
	if (mtu <= 0) {
		LOG_ERROR("failed to get local interface MTU. Assume a MTU of %d", max_pmtu);
	} else {
		max_pmtu = mtu;
		LOG_INFO("max_pmtu = mtu of local interface = %d", max_pmtu);
	}
	
	pthread_create(&listener_thread_id, NULL, listener, NULL);
	//Pthread_detach(threadId);
	
	state = BASE;
	for (;;) {
		state = run_state(state);
	}
	
	LOG_DEBUG("%s - leave controller", THREAD_NAME);
	return NULL;
}

pthread_t dplpmtud_start(int socket, int address_family, int listener_only) {
	LOG_DEBUG("%s - dplpmtud_start entered", THREAD_NAME);
	if (main_thread_id != NULL) {
		// dplpmtud thread already started
		LOG_INFO("%s - dplpmtud thread already started", THREAD_NAME);
		LOG_DEBUG("%s - leave dplpmtud_start", THREAD_NAME);
		return NULL;
	}
	if (address_family == AF_INET) {
		dplpmtud_ip_version = IPv4;
	} else if (address_family == AF_INET6) {
		dplpmtud_ip_version = IPv6;
	} else {
		LOG_ERROR("%s - unknown address family", THREAD_NAME);
		LOG_DEBUG("%s - leave dplpmtud_start", THREAD_NAME);
		return NULL;
	}
	dplpmtud_socket = socket;
	heartbeat_response_received_mutex = PTHREAD_MUTEX_INITIALIZER;
	heartbeat_response_received_cond = PTHREAD_COND_INITIALIZER;
	heartbeat_response_received = 0;
	dplpmtud_listener_only = listener_only;
	
	pthread_create(&main_thread_id, NULL, controller, NULL);
	LOG_DEBUG("%s - leave dplpmtud_start", THREAD_NAME);
	return main_thread_id;
}

void dplpmtud_wait() {
	LOG_DEBUG("%s - dplpmtud_wait entered", THREAD_NAME);
	if (main_thread_id != NULL) {
		pthread_join(main_thread_id, NULL);
	}
	LOG_DEBUG("%s - leave dplpmtud_wait", THREAD_NAME);
}
