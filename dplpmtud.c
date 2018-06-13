
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

#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>

// TODO: Split source file
// TODO: increase probe_size better

#define BASE_PMTU_IPv6 1280
#define BASE_PMTU_IPv4 1200
#define BASE_PMTU ((dplpmtud_ip_version == IPv4) ? BASE_PMTU_IPv4 : BASE_PMTU_IPv6)
#define MAX_PROBES 10
#define MIN_PMTU_IPv6 1280
#define MIN_PMTU_IPv4 68
#define MIN_PMTU ((dplpmtud_ip_version == IPv4) ? MIN_PMTU_IPv4 : MIN_PMTU_IPv6)

#define ICMP4_HEADER_SIZE 8
#define ICMP6_HEADER_SIZE 8

#define PROBE_TIMEOUT 20
#define REACHABILITY_TIMEOUT 100
#define RAISE_TIMEOUT 600

#define BUFFER_SIZE (1<<16)

static uint32_t probe_size;
static uint32_t probed_size_success;
static pthread_t main_thread_id = NULL;
static pthread_t listener_thread_id;
static pthread_t prober_thread_id;
static pthread_t ptb_listener_thread_id;
static int dplpmtud_socket = 0;
static int dplpmtud_passive_mode;
static int dplpmtud_handle_ptb;
static uint32_t max_pmtu = 1500;
static int icmp_socket;

pthread_mutex_t probe_return_mutex;
pthread_cond_t probe_return_cond;
uint32_t volatile probe_return;

pthread_mutex_t probe_sequence_number_mutex;
uint32_t volatile probe_sequence_number;

uint32_t ptb_mtu;

pthread_mutex_t ptb_mtu_limit_mutex;
uint32_t volatile ptb_mtu_limit;

#define THREAD_NAME ((pthread_self() == prober_thread_id ? "prober_thread" : (pthread_self() == main_thread_id ? "main_thread" : (pthread_self() == ptb_listener_thread_id ? "ptb_listener_thread" : "caller_thread"))))

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
	LOG_DEBUG("%s - do_probe entered", THREAD_NAME);
	uint32_t probe_count;
	struct timespec stop_time;
	struct timeval now;
	
	probe_count = 0;
	
	pthread_mutex_lock(&probe_return_mutex);
	LOG_DEBUG("%s - mutex locked", THREAD_NAME);
	while (probe_count < MAX_PROBES) {
		increment_probe_sequence_number();
		probe_return = 0;
		LOG_INFO("%s - probe %u bytes with seq_no= %u", THREAD_NAME, probe_size, probe_sequence_number);
		probe_count++;
		if (send_probe(dplpmtud_socket, probe_size) < 0) {
			LOG_PERROR("%s - send_probe", THREAD_NAME);
		} else {
			LOG_DEBUG("%s - cond wait", THREAD_NAME);
			gettimeofday(&now, NULL);
			stop_time.tv_sec = now.tv_sec+PROBE_TIMEOUT;
			stop_time.tv_nsec = now.tv_usec * 1000;
			pthread_cond_timedwait(&probe_return_cond, &probe_return_mutex, &stop_time); 
		}
		LOG_DEBUG("%s - probe_return == %d", THREAD_NAME, probe_return);
		if (probe_return == 1) { /* heartbeat response received */
			LOG_INFO("%s - probe with %u bytes succeeded", THREAD_NAME, probe_size);
			probed_size_success = probe_size;
			
			pthread_mutex_unlock(&probe_return_mutex);
			LOG_DEBUG("%s - leave do_probe", THREAD_NAME);
			return 1;
		} else if (probe_return > 1) { /* valid PTB received */
			ptb_mtu = probe_return;
			pthread_mutex_unlock(&probe_return_mutex);
			LOG_DEBUG("%s - leave do_probe", THREAD_NAME);
			return 2;
		} 
	}
	pthread_mutex_unlock(&probe_return_mutex);
	LOG_INFO("%s - probe with %u bytes failed", THREAD_NAME, probe_size);
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
	LOG_DEBUG("%s - probe_return = 1", THREAD_NAME);
	pthread_cond_signal(&probe_return_cond);
	pthread_mutex_unlock(&probe_return_mutex);
	return 1;
}

static int signal_probe_return_with_mtu(uint32_t mtu) {
	LOG_DEBUG("%s - enter signal_probe_return_with_mtu", THREAD_NAME);
	uint32_t mtu_limit;
	mtu_limit = get_ptb_mtu_limit();
	if ( !(mtu_limit == 0 || (1 < mtu && mtu < mtu_limit)) ) {
		LOG_DEBUG("%s - do not signal probe return. %u, %u", THREAD_NAME, mtu, mtu_limit);
		return 0;
	}
	LOG_DEBUG("%s - signal probe return with mtu %u", THREAD_NAME, mtu);
	pthread_mutex_lock(&probe_return_mutex);
	probe_return = mtu;
	LOG_DEBUG("%s - probe_return = %u", THREAD_NAME, mtu);
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
	LOG_DEBUG("%s - run_base_state entered", THREAD_NAME);
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
	LOG_ERROR("%s - do_probe with unexpected return value %d", THREAD_NAME, probe_value);
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
	LOG_ERROR("%s - do_probe with unexpected return value %d", THREAD_NAME, probe_value);
	return DISABLED;
}

state_t run_done_state() {
	LOG_DEBUG("%s - run_done_state entered", THREAD_NAME);
	int probe_success;
	time_t raise_timer_start;
	
	set_ptb_mtu_limit(1);
	probe_size = probed_size_success;
	raise_timer_start = time(NULL);
	probe_success = 1;
	while (probe_success) {
		LOG_DEBUG("%s - sleep for REACHABILITY_TIMEOUT", THREAD_NAME);
		sleep(REACHABILITY_TIMEOUT);
		if (is_raise_timer_expired(raise_timer_start)) break;
		probe_success = do_probe();
		if (is_raise_timer_expired(raise_timer_start)) break;
	}
	
	LOG_DEBUG("%s - leave run_done_state", THREAD_NAME);
	return BASE;
}

state_t run_error_state() {
	LOG_DEBUG("%s - run_error_state entered", THREAD_NAME);
	int probe_success;
	
	probe_size = MIN_PMTU;
	set_ptb_mtu_limit(1);
	probe_success = 0;
	while (!probe_success) {
		probe_success = do_probe();
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

static void *prober(void *arg) {
	LOG_DEBUG("%s - prober entered", THREAD_NAME);
	state_t state;
	int mtu;
	
	mtu = get_local_if_mtu(dplpmtud_socket);
	if (mtu <= 0) {
		LOG_ERROR("failed to get local interface MTU. Assume a MTU of %d", max_pmtu);
	} else {
		max_pmtu = mtu;
		LOG_INFO("max_pmtu = mtu of local interface = %d", max_pmtu);
	}
	int val = 1;
	if (setsockopt(dplpmtud_socket, IPPROTO_IP, IP_DONTFRAG, &val, sizeof(val)) != 0) {
		LOG_PERROR("setsockopt IP_DONTFRAG");
	}
	if (setsockopt(dplpmtud_socket, IPPROTO_IPV6, IPV6_DONTFRAG, &val, sizeof(val)) != 0) {
		LOG_PERROR("setsockopt IPV6_DONTFRAG");
	}
	
	state = BASE;
	while (state != DISABLED) {
		state = run_state(state);
	}
	
	LOG_DEBUG("%s - leave prober", THREAD_NAME);
	return NULL;
}

static void *ptb_listener(void *arg) {
	LOG_DEBUG("%s - ptb listener entered", THREAD_NAME);
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
				LOG_DEBUG("did not receive the full ip packet. %d!=%zd", ntohs(ip_header->ip_len), recv_len);
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
	LOG_DEBUG("%s - leave ptb listener", THREAD_NAME);
	return NULL;
}

static void *controller(void *arg) {
	LOG_DEBUG("%s - controller entered", THREAD_NAME);
	struct icmp6_filter icmp6_filt;
	
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
			setuid(getuid());
			pthread_create(&ptb_listener_thread_id, NULL, ptb_listener, NULL);
		}
		pthread_create(&prober_thread_id, NULL, prober, NULL);
	}
	
	listener_thread_id = main_thread_id;
	LOG_DEBUG("%s - leave controller", THREAD_NAME);
	return listener(NULL);
}

pthread_t dplpmtud_start(int socket, int address_family, int passive_mode, int handle_ptb) {
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
	probe_return_mutex = PTHREAD_MUTEX_INITIALIZER;
	probe_return_cond = PTHREAD_COND_INITIALIZER;
	probe_return = 0;
	probe_sequence_number_mutex = PTHREAD_MUTEX_INITIALIZER;
	probe_sequence_number = 0;
	ptb_mtu_limit_mutex = PTHREAD_MUTEX_INITIALIZER;
	ptb_mtu_limit = 1;
	
	dplpmtud_passive_mode = passive_mode;
	if (passive_mode) {
		dplpmtud_handle_ptb = 0;
	} else {
		dplpmtud_handle_ptb = handle_ptb;
	}
	
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
