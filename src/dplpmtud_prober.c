
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
#include "dplpmtud_util.h"
#include "dplpmtud_main.h"
#include "cblib.h"

#define BASE_PMTU_IPv6 1280
#define BASE_PMTU_IPv4 1200
#define BASE_PMTU ((dplpmtud_ip_version == IPv4) ? BASE_PMTU_IPv4 : BASE_PMTU_IPv6)
#define MAX_PROBES 10
#define MIN_PMTU_IPv6 1280
#define MIN_PMTU_IPv4 68
#define MIN_PMTU ((dplpmtud_ip_version == IPv4) ? MIN_PMTU_IPv4 : MIN_PMTU_IPv6)
#define MAX_PMTU 16384

#define PROBE_TIMEOUT 20
#define VALIDATION_TIMEOUT 100
#define MAX_VALIDATION 5

typedef enum { 
	START=0,
	DISABLED,
	BASE,
	SEARCH,
	ERROR,
	DONE 
} state_t;

typedef void state_run_func_t();
typedef void state_probe_acked_func_t();
typedef void state_probe_failed_func_t();
typedef void state_ptb_received_func_t(uint32_t);

static void start_run();
static void start_probe_acked();
static void start_probe_failed();
static void disabled_run();
static void base_run();
static void base_probe_acked();
static void base_probe_failed();
static void base_ptb_received(uint32_t);
static void search_run();
static void search_probe_acked();
static void search_probe_failed();
static void search_ptb_received(uint32_t);
static void error_run();
static void error_probe_acked();
static void error_probe_failed();
static void done_run();
static void done_probe_acked();
static void done_probe_failed();
static void done_ptb_received(uint32_t);

state_run_func_t* const state_run_table[] = {
	start_run, 
	disabled_run,
	base_run, 
	search_run, 
	error_run,
	done_run
};
state_probe_acked_func_t* const state_probe_acked_table[] = {
	start_probe_acked, 
	NULL,
	base_probe_acked, 
	search_probe_acked, 
	error_probe_acked,
	done_probe_acked
};
state_probe_failed_func_t* const state_probe_failed_table[] = {
	start_probe_failed, 
	NULL,
	base_probe_failed, 
	search_probe_failed, 
	error_probe_failed,
	done_probe_failed
};
state_ptb_received_func_t* const state_ptb_received_table[] = {
	NULL, 
	NULL,
	base_ptb_received, 
	search_ptb_received, 
	NULL,
	done_ptb_received
};

static int dplpmtud_socket;
static uint32_t probe_size;
static uint32_t probed_size;
static uint32_t current_max_pmtu;
static uint32_t probe_sequence_number;
static uint32_t ptb_mtu_limit;
static uint32_t probe_count;
static uint32_t validation_count;
static int remote_if_mtu;
static state_t state;
static struct timer *probe_timer;
static struct timer *validation_timer;


static void increase_probe_size() {
	probe_size += 50;
	if (probe_size > current_max_pmtu) {
		probe_size = current_max_pmtu;
	}
}

uint32_t get_probe_sequence_number() {
	return probe_sequence_number;
}

static void send_probe(int flags) {
	LOG_TRACE_ENTER
	probe_sequence_number++;
	LOG_INFO_("probe %u bytes with seq_no= %u", probe_size, probe_sequence_number);
	if (dplpmtud_send_probe(dplpmtud_socket, probe_size, flags) < 0) {
		LOG_PERROR("dplpmtud_send_probe");
		if (state_probe_failed_table[state] != NULL) {
			(*state_probe_failed_table[state])();
		}
	} else {
		probe_count++;
		start_timer(probe_timer, PROBE_TIMEOUT*1000);
	}
	LOG_TRACE_LEAVE
}

void update_max_pmtu() {
	int if_mtu;
	
	if_mtu = get_local_if_mtu(dplpmtud_socket);
	if (if_mtu <= 0) {
		LOG_ERROR("failed to get local interface MTU.");
		if_mtu = remote_if_mtu;
	} else if (0 < remote_if_mtu && remote_if_mtu < if_mtu) {
		if_mtu = remote_if_mtu;
	}
	if (if_mtu <= 0) {
		current_max_pmtu = 1500;
		LOG_INFO_("No information about interface MTU. Assume a MTU of %d", current_max_pmtu);
	} else if (if_mtu > MAX_PMTU) {
		current_max_pmtu = MAX_PMTU;
		LOG_INFO_("current_max_pmtu = MAX_PMTU = %d", current_max_pmtu);
	} else {
		current_max_pmtu = if_mtu;
		LOG_INFO_("current_max_pmtu = min(local_if_mtu, remote_if_mtu) = %d", current_max_pmtu);
	}
}


static void disabled_run() {
	LOG_TRACE_ENTER
	state = DISABLED;
	LOG_TRACE_LEAVE
}


static void start_run() {
	LOG_TRACE_ENTER
	state = START;
	probe_size = 0;
	probe_count = 0;
	send_probe(0);
	LOG_TRACE_LEAVE
}

static void start_probe_acked() {
	LOG_TRACE_ENTER
	base_run();
	LOG_TRACE_LEAVE
}

static void start_probe_failed() {
	LOG_TRACE_ENTER
	disabled_run();
	LOG_TRACE_LEAVE
}


static void base_run() {
	LOG_TRACE_ENTER
	state = BASE;
	probe_size = BASE_PMTU;
	probe_count = 0;
	remote_if_mtu = 0;
	send_probe(1);
	LOG_TRACE_LEAVE
}

static void base_probe_acked() {
	LOG_TRACE_ENTER
	search_run();
	LOG_TRACE_LEAVE
}

static void base_probe_failed() {
	LOG_TRACE_ENTER
	error_run();
	LOG_TRACE_LEAVE
}

static void base_ptb_received(uint32_t ptb_mtu) {
	LOG_TRACE_ENTER
	if (ptb_mtu < BASE_PMTU) {
		error_run();
	} else {
		done_run();
	}
	LOG_TRACE_LEAVE
}


static void search_run() {
	LOG_TRACE_ENTER
	state = SEARCH;
	update_max_pmtu();
	probed_size = probe_size;
	increase_probe_size();
	ptb_mtu_limit = probe_size;
	probe_count = 0;
	send_probe(0);
	LOG_TRACE_LEAVE
}

static void search_probe_acked() {
	LOG_TRACE_ENTER
	probed_size = probe_size;
	if (probed_size == current_max_pmtu) {
		done_run();
	} else {
		increase_probe_size();
		ptb_mtu_limit = probe_size;
		probe_count = 0;
		send_probe(0);
	}
	LOG_TRACE_LEAVE
}

static void search_probe_failed() {
	LOG_TRACE_ENTER
	done_run();
	LOG_TRACE_LEAVE
}

static void search_ptb_received(uint32_t ptb_mtu) {
	LOG_TRACE_ENTER
	if (ptb_mtu < probed_size) {
		base_run();
	} else if (ptb_mtu < probe_size) {
		probe_size = ptb_mtu;
		current_max_pmtu = ptb_mtu;
		if (probed_size == current_max_pmtu) {
			done_run();
		} else {
			probe_count = 0;
			send_probe(0);
		}
	}
	LOG_TRACE_LEAVE
}


static void error_run() {
	LOG_TRACE_ENTER
	state = ERROR;
	probe_size = MIN_PMTU;
	probe_count = 0;
	remote_if_mtu = 0;
	send_probe(1);
	LOG_TRACE_LEAVE
}

static void error_probe_acked() {
	LOG_TRACE_ENTER
	search_run();
	LOG_TRACE_LEAVE
}

static void error_probe_failed() {
	LOG_TRACE_ENTER
	probe_count = 0;
	send_probe(0);
	LOG_TRACE_LEAVE
}


static void done_run() {
	LOG_TRACE_ENTER
	state = DONE;
	probe_size = probed_size;
	ptb_mtu_limit = probed_size;
	validation_count = 0;
	start_timer(validation_timer, VALIDATION_TIMEOUT*1000);
	LOG_TRACE_LEAVE
}

static void done_probe_acked() {
	LOG_TRACE_ENTER
	if (validation_count < MAX_VALIDATION) {
		start_timer(validation_timer, VALIDATION_TIMEOUT*1000);
	} else {
		search_run();
	}
	LOG_TRACE_LEAVE
}

static void done_probe_failed() {
	LOG_TRACE_ENTER
	base_run();
	LOG_TRACE_LEAVE
}

static void done_ptb_received(uint32_t ptb_mtu) {
	LOG_TRACE_ENTER
	if (ptb_mtu < probed_size) {
		base_run();
	}
	LOG_TRACE_LEAVE
}


void dplpmtud_remote_if_mtu_received(int mtu) {
	LOG_TRACE_ENTER
	remote_if_mtu = mtu;
	LOG_DEBUG_("remote_if_mtu=%d", remote_if_mtu);
	LOG_TRACE_LEAVE
}

void dplpmtud_ptb_received(uint32_t ptb_mtu) {
	LOG_TRACE_ENTER
	LOG_INFO_("PTB with MTU %u received", ptb_mtu);
	if (state_ptb_received_table[state] != NULL) {
		if (ptb_mtu_limit == 0 || ptb_mtu < ptb_mtu_limit) {
			LOG_INFO("handle PTB");
			stop_timer(probe_timer);
		}
		(*state_ptb_received_table[state])(ptb_mtu);
	} else {
		LOG_INFO("ignore PTB");
	}
	LOG_TRACE_LEAVE
}

void dplpmtud_probe_acked() {
	LOG_TRACE_ENTER
	stop_timer(probe_timer);
	LOG_INFO_("probe with %u acked", probe_size);
	if (state_probe_acked_table[state] != NULL) {
		(*state_probe_acked_table[state])();
	}
	LOG_TRACE_LEAVE
}

static void on_probe_timer_expired(void *arg) {
	LOG_TRACE_ENTER
	if (probe_count == MAX_PROBES) {
		LOG_INFO_("probe with %u failed", probe_size);
		if (state_probe_failed_table[state] != NULL) {
			(*state_probe_failed_table[state])();
		}
	} else {
		send_probe(0);
	}
	LOG_TRACE_LEAVE
}

static void on_validation_timer_expired(void *arg) {
	LOG_TRACE_ENTER
	validation_count++;
	probe_count = 0;
	if (validation_count < MAX_VALIDATION) {
		send_probe(0);
	} else {
		remote_if_mtu = 0;
		send_probe(1);
	}
	LOG_TRACE_LEAVE
}

int dplpmtud_start_prober(int socket) {
	LOG_TRACE_ENTER
	
	dplpmtud_socket = socket;
	
	probe_timer = create_timer(&on_probe_timer_expired, NULL, "probe timer");
	validation_timer = create_timer(&on_validation_timer_expired, NULL, "validation timer");
	
	int val = 1;
	if (dplpmtud_ip_version == IPv4) {
		if (set_ip_dont_fragment_option(dplpmtud_socket) != 0) {
			LOG_PERROR("set_ip_dont_fragment_option");
			disabled_run();
			return -1;
		}
	} else {
		if (setsockopt(dplpmtud_socket, IPPROTO_IPV6, IPV6_DONTFRAG, &val, sizeof(val)) != 0) {
			LOG_PERROR("setsockopt IPV6_DONTFRAG");
			disabled_run();
			return -1;
		}
	}
	
	ptb_mtu_limit = 0;
	probe_sequence_number = 0;
	remote_if_mtu = 0;
	start_run();
	LOG_TRACE_LEAVE
	return 0;
}
