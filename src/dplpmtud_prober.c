
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
#include "dplpmtud_util.h"
#include "dplpmtud_main.h"

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

static uint32_t probe_size;
static uint32_t probed_size_success;
static uint32_t max_pmtu = 1500;

static pthread_mutex_t probe_return_mutex;
static pthread_cond_t probe_return_cond;
static uint32_t volatile probe_return;

static pthread_mutex_t probe_sequence_number_mutex;
static uint32_t volatile probe_sequence_number;

static uint32_t ptb_mtu;

static pthread_mutex_t ptb_mtu_limit_mutex;
static uint32_t volatile ptb_mtu_limit;

void dplpmtud_prober_init() {
	probe_return_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	probe_return_cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	probe_return = 0;
	probe_sequence_number_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	probe_sequence_number = 0;
	ptb_mtu_limit_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
	ptb_mtu_limit = 1;
}

static void increase_probe_size() {
	probe_size += 50;
	if (probe_size > max_pmtu) {
		probe_size = max_pmtu;
	}
}

static void increment_probe_sequence_number() {
	pthread_mutex_lock(&probe_sequence_number_mutex);
	probe_sequence_number++;
	pthread_mutex_unlock(&probe_sequence_number_mutex);
}

static int do_probe() {
	LOG_DEBUG_("%s - do_probe entered", THREAD_NAME);
	uint32_t probe_count;
	struct timespec stop_time;
	struct timeval now;
	
	probe_count = 0;
	
	pthread_mutex_lock(&probe_return_mutex);
	LOG_DEBUG_("%s - mutex locked", THREAD_NAME);
	while (probe_count < MAX_PROBES) {
		increment_probe_sequence_number();
		probe_return = 0;
		LOG_INFO_("%s - probe %u bytes with seq_no= %u", THREAD_NAME, probe_size, probe_sequence_number);
		probe_count++;
		if (send_probe(dplpmtud_socket, probe_size) < 0) {
			LOG_PERROR_("%s - send_probe", THREAD_NAME);
		} else {
			LOG_DEBUG_("%s - cond wait", THREAD_NAME);
			gettimeofday(&now, NULL);
			stop_time.tv_sec = now.tv_sec+PROBE_TIMEOUT;
			stop_time.tv_nsec = now.tv_usec * 1000;
			pthread_cond_timedwait(&probe_return_cond, &probe_return_mutex, &stop_time); 
		}
		LOG_DEBUG_("%s - probe_return == %d", THREAD_NAME, probe_return);
		if (probe_return == 1) { /* heartbeat response received */
			LOG_INFO_("%s - probe with %u bytes succeeded", THREAD_NAME, probe_size);
			probed_size_success = probe_size;
			
			pthread_mutex_unlock(&probe_return_mutex);
			LOG_DEBUG_("%s - leave do_probe", THREAD_NAME);
			return 1;
		} else if (probe_return > 1) { /* valid PTB received */
			ptb_mtu = probe_return;
			pthread_mutex_unlock(&probe_return_mutex);
			LOG_DEBUG_("%s - leave do_probe", THREAD_NAME);
			return 2;
		} 
	}
	pthread_mutex_unlock(&probe_return_mutex);
	LOG_INFO_("%s - probe with %u bytes failed", THREAD_NAME, probe_size);
	LOG_DEBUG_("%s - leave do_probe", THREAD_NAME);
	return 0;
}

static int is_raise_timer_expired(time_t raise_timer_start) {
	if ((time(NULL) - raise_timer_start) >= RAISE_TIMEOUT) {
		LOG_DEBUG_("%s - RAISE_TIMER expired", THREAD_NAME);
		return 1;
	}
	return 0;
}

static void set_ptb_mtu_limit(uint32_t mtu_limit) {
	pthread_mutex_lock(&ptb_mtu_limit_mutex);
	ptb_mtu_limit = mtu_limit;
	pthread_mutex_unlock(&ptb_mtu_limit_mutex);
}

static uint32_t get_ptb_mtu_limit() {
	uint32_t mtu_limit;
	pthread_mutex_lock(&ptb_mtu_limit_mutex);
	mtu_limit = ptb_mtu_limit;
	pthread_mutex_unlock(&ptb_mtu_limit_mutex);
	return mtu_limit;
}

int signal_probe_return() {
	pthread_mutex_lock(&probe_return_mutex);
	probe_return = 1;
	LOG_DEBUG_("%s - probe_return = 1", THREAD_NAME);
	pthread_cond_signal(&probe_return_cond);
	pthread_mutex_unlock(&probe_return_mutex);
	return 1;
}

int signal_probe_return_with_mtu(uint32_t mtu) {
	LOG_DEBUG_("%s - enter signal_probe_return_with_mtu", THREAD_NAME);
	uint32_t mtu_limit;
	mtu_limit = get_ptb_mtu_limit();
	if ( !(mtu_limit == 0 || (1 < mtu && mtu < mtu_limit)) ) {
		LOG_DEBUG_("%s - do not signal probe return. %u, %u", THREAD_NAME, mtu, mtu_limit);
		return 0;
	}
	LOG_DEBUG_("%s - signal probe return with mtu %u", THREAD_NAME, mtu);
	pthread_mutex_lock(&probe_return_mutex);
	probe_return = mtu;
	LOG_DEBUG_("%s - probe_return = %u", THREAD_NAME, mtu);
	pthread_cond_signal(&probe_return_cond);
	pthread_mutex_unlock(&probe_return_mutex);
	return 1;
}

uint32_t get_probe_sequence_number() {
	uint32_t seq_no;
	pthread_mutex_lock(&probe_sequence_number_mutex);
	seq_no = probe_sequence_number;
	pthread_mutex_unlock(&probe_sequence_number_mutex);
	return seq_no;
}

state_t run_base_state() {
	LOG_DEBUG_("%s - run_base_state entered", THREAD_NAME);
	int probe_value;
	
	probe_size = BASE_PMTU;
	set_ptb_mtu_limit(0);
	probe_value = do_probe();
	switch (probe_value) {
		case 0: /* no response */
			return ERROR;
		case 1: /* heartbeat response received */
			return SEARCH;
		case 2: /* PTB received */
			if (ptb_mtu < BASE_PMTU) {
				return ERROR;
			} else {
				return DONE;
			}
	}
	LOG_ERROR_("%s - do_probe with unexpected return value %d", THREAD_NAME, probe_value);
	return DISABLED;
}

state_t run_search_state() {
	int probe_value;
	
	probe_value = 1;
	while (probe_value == 1 || probe_value == 2) {
		set_ptb_mtu_limit(probe_size);
		probe_value = do_probe();
		switch (probe_value) {
			case 0: /* no response */
				return DONE;
			case 1: /* heartbeat response received */
				if (probed_size_success == max_pmtu) {
					return DONE;
				}
				increase_probe_size();
				break;
			case 2: /* PTB with MTU < probe_size */
				//return BASE;
				probe_size = ptb_mtu;
				max_pmtu = ptb_mtu;
		}
	}
	LOG_ERROR_("%s - do_probe with unexpected return value %d", THREAD_NAME, probe_value);
	return DISABLED;
}

state_t run_done_state() {
	LOG_DEBUG_("%s - run_done_state entered", THREAD_NAME);
	int probe_success;
	time_t raise_timer_start;
	
	set_ptb_mtu_limit(1);
	probe_size = probed_size_success;
	raise_timer_start = time(NULL);
	probe_success = 1;
	while (probe_success) {
		LOG_DEBUG_("%s - sleep for REACHABILITY_TIMEOUT", THREAD_NAME);
		sleep(REACHABILITY_TIMEOUT);
		if (is_raise_timer_expired(raise_timer_start)) break;
		probe_success = do_probe();
		if (is_raise_timer_expired(raise_timer_start)) break;
	}
	
	LOG_DEBUG_("%s - leave run_done_state", THREAD_NAME);
	return BASE;
}

state_t run_error_state() {
	LOG_DEBUG_("%s - run_error_state entered", THREAD_NAME);
	int probe_success;
	
	probe_size = MIN_PMTU;
	set_ptb_mtu_limit(1);
	probe_success = 0;
	while (!probe_success) {
		probe_success = do_probe();
	}
	
	LOG_DEBUG_("%s - leave run_error_state", THREAD_NAME);
	return SEARCH;
}

void *dplpmtud_prober(void *arg) {
	LOG_DEBUG_("%s - prober entered", THREAD_NAME);
	state_t state;
	int mtu;
	
	mtu = get_local_if_mtu(dplpmtud_socket);
	if (mtu <= 0) {
		LOG_ERROR_("failed to get local interface MTU. Assume a MTU of %d", max_pmtu);
	} else {
		max_pmtu = mtu;
		LOG_INFO_("max_pmtu = mtu of local interface = %d", max_pmtu);
	}
	int val = 1;
	if (dplpmtud_ip_version == IPv4) {
		if (set_ip_dont_fragment_option(dplpmtud_socket) != 0) {
			LOG_PERROR("setsockopt IP_DONTFRAG");
		}
	} else {
		if (setsockopt(dplpmtud_socket, IPPROTO_IPV6, IPV6_DONTFRAG, &val, sizeof(val)) != 0) {
			LOG_PERROR("setsockopt IPV6_DONTFRAG");
		}
	}
	
	state = BASE;
	while (state != DISABLED) {
		state = run_state(state);
	}
	
	LOG_DEBUG_("%s - leave prober", THREAD_NAME);
	return NULL;
}
