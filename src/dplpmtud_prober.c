
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

static void send_probe() {
	LOG_DEBUG("send_probe entered");
	LOG_INFO_("probe %u bytes with seq_no= %u", probe_size, probe_sequence_number);
	if (dplpmtud_send_probe(dplpmtud_socket, probe_size) < 0) {
		LOG_PERROR("dplpmtud_send_probe");
		(*state_probe_failed_table[state])();
	} else {
		probe_sequence_number++;
		probe_count++;
		start_timer(probe_timer, PROBE_TIMEOUT*1000);
	}
	LOG_DEBUG("leave send_probe");
}

void update_max_pmtu() {
	int mtu;
	
	current_max_pmtu = 1500;
	mtu = get_local_if_mtu(dplpmtud_socket);
	if (mtu <= 0) {
		LOG_ERROR_("failed to get local interface MTU. Assume a MTU of %d", current_max_pmtu);
	} else {
		if (MAX_PMTU < mtu) {
			current_max_pmtu = MAX_PMTU;
			LOG_INFO_("current_max_pmtu = MAX_PMTU = %d", current_max_pmtu);
		} else {
			current_max_pmtu = mtu;
			LOG_INFO_("current_max_pmtu = mtu of local interface = %d", current_max_pmtu);
		}
	}
}


static void disabled_run() {
	LOG_DEBUG("disabled_run entered");
	state = DISABLED;
	LOG_DEBUG("leave disabled_run");
}


static void start_run() {
	LOG_DEBUG("start_run entered");
	state = START;
	probe_size = 0;
	probe_count = 0;
	send_probe();
	LOG_DEBUG("leave start_run");
}

static void start_probe_acked() {
	LOG_DEBUG("start_probe_acked entered");
	base_run();
	LOG_DEBUG("leave start_probe_acked");
}

static void start_probe_failed() {
	LOG_DEBUG("start_probe_failed entered");
	disabled_run();
	LOG_DEBUG("leave start_probe_failed");
}


static void base_run() {
	LOG_DEBUG("base_run entered");
	state = BASE;
	probe_size = BASE_PMTU;
	probe_count = 0;
	send_probe();
	LOG_DEBUG("leave base_run");
}

static void base_probe_acked() {
	LOG_DEBUG("base_probe_acked entered");
	search_run();
	LOG_DEBUG("leave base_probe_acked");
}

static void base_probe_failed() {
	LOG_DEBUG("base_probe_failed entered");
	error_run();
	LOG_DEBUG("leave base_probe_failed");
}

static void base_ptb_received(uint32_t ptb_mtu) {
	LOG_DEBUG("base_ptb_received entered");
	if (ptb_mtu < BASE_PMTU) {
		error_run();
	} else {
		done_run();
	}
	LOG_DEBUG("leave base_ptb_received");
}


static void search_run() {
	LOG_DEBUG("search_run entered");
	state = SEARCH;
	update_max_pmtu();
	increase_probe_size();
	ptb_mtu_limit = probe_size;
	probe_count = 0;
	send_probe();
	LOG_DEBUG("leave search_run");
}

static void search_probe_acked() {
	LOG_DEBUG("search_probe_acked entered");
	probed_size = probe_size;
	if (probed_size == current_max_pmtu) {
		done_run();
	} else {
		increase_probe_size();
		probe_count = 0;
		send_probe();
	}
	LOG_DEBUG("leave search_probe_acked");
}

static void search_probe_failed() {
	LOG_DEBUG("search_probe_failed entered");
	done_run();
	LOG_DEBUG("leave search_probe_failed");
}

static void search_ptb_received(uint32_t ptb_mtu) {
	LOG_DEBUG("search_ptb_received entered");
	if (ptb_mtu < probed_size) {
		base_run();
	} else if (ptb_mtu < probe_size) {
		probe_size = ptb_mtu;
		current_max_pmtu = ptb_mtu;
		if (probed_size == current_max_pmtu) {
			done_run();
		} else {
			probe_count = 0;
			send_probe();
		}
	}
	LOG_DEBUG("leave search_ptb_received");
}


static void error_run() {
	LOG_DEBUG("error_run entered");
	state = ERROR;
	probe_size = MIN_PMTU;
	probe_count = 0;
	send_probe();
	LOG_DEBUG("leave error_run");
}

static void error_probe_acked() {
	LOG_DEBUG("error_probe_acked entered");
	search_run();
	LOG_DEBUG("leave error_probe_acked");
}

static void error_probe_failed() {
	LOG_DEBUG("error_probe_failed entered");
	probe_count = 0;
	send_probe();
	LOG_DEBUG("leave error_probe_failed");
}


static void done_run() {
	LOG_DEBUG("done_run entered");
	state = DONE;
	probe_size = probed_size;
	ptb_mtu_limit = probed_size;
	start_timer(validation_timer, VALIDATION_TIMEOUT*1000);
	LOG_DEBUG("leave done_run");
}

static void done_probe_acked() {
	LOG_DEBUG("done_probe_acked entered");
	if (validation_count < MAX_VALIDATION) {
		start_timer(validation_timer, VALIDATION_TIMEOUT*1000);
	} else {
		search_run();
	}
	LOG_DEBUG("leave done_probe_acked");
}

static void done_probe_failed() {
	LOG_DEBUG("done_probe_failed entered");
	base_run();
	LOG_DEBUG("leave done_probe_failed");
}

static void done_ptb_received(uint32_t ptb_mtu) {
	LOG_DEBUG("done_ptb_received entered");
	if (ptb_mtu < probed_size) {
		base_run();
	}
	LOG_DEBUG("leave done_ptb_received");
}


void dplpmtud_ptb_received(uint32_t ptb_mtu) {
	LOG_DEBUG("dplpmtud_ptb_received entered");
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
	LOG_DEBUG("leave dplpmtud_ptb_received");
}

void dplpmtud_probe_acked() {
	LOG_DEBUG("dplpmtud_probe_acked entered");
	stop_timer(probe_timer);
	LOG_INFO_("probe with %u acked", probe_size);
	(*state_probe_acked_table[state])();
	// call state_run
	LOG_DEBUG("leave dplpmtud_probe_acked");
}

static void on_probe_timer_expired(void *arg) {
	LOG_DEBUG("on_probe_timer_expired entered");
	if (probe_count == MAX_PROBES) {
		LOG_INFO_("probe with %u failed", probe_size);
		(*state_probe_failed_table[state])();
	} else {
		send_probe();
	}
	LOG_DEBUG("leave on_probe_timer_expired");
}

static void on_validation_timer_expired(void *arg) {
	LOG_DEBUG("on_validation_timer_expired entered");
	validation_count++;
	probe_count = 0;
	send_probe();
	LOG_DEBUG("leave on_validation_timer_expired");
}

int dplpmtud_start_prober(int socket) {
	LOG_DEBUG("dplpmtud_start_prober entered");
	
	dplpmtud_socket = socket;
	
	probe_timer = create_timer(&on_probe_timer_expired, NULL, "probe timer");
	validation_timer = create_timer(&on_validation_timer_expired, NULL, "validation timer");
	
	update_max_pmtu();
	int val = 1;
	if (dplpmtud_ip_version == IPv4) {
		if (set_ip_dont_fragment_option(dplpmtud_socket) != 0) {
			LOG_PERROR("setsockopt IP_DONTFRAG");
			return -1;
		}
	} else {
		if (setsockopt(dplpmtud_socket, IPPROTO_IPV6, IPV6_DONTFRAG, &val, sizeof(val)) != 0) {
			LOG_PERROR("setsockopt IPV6_DONTFRAG");
			return -1;
		}
	}
	
	ptb_mtu_limit = 0;
	probe_sequence_number = 0;
	start_run();
	LOG_DEBUG("leave dplpmtud_start_prober");
	return 0;
}
