#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include "logger.h"
#include "dplpmtud_main.h"
#include "dplpmtud_prober.h"
#include "dplpmtud_ptb_handler.h"

#include "cblib.h"
#include "dplpmtud_pl.h"

// TODO: increase probe_size better
// TODO assuming a fix IP header size in dplpmtud_pl_udp OK?

#define BUFFER_SIZE (1<<16)

int dplpmtud_socket = 0;

static pthread_t dplpmtud_thread_id = 0;
static int dplpmtud_passive_mode;
static int dplpmtud_handle_ptb;

void dplpmtud_socket_readable(void *arg) {
	LOG_TRACE_ENTER
	ssize_t recv_len;
	char buf[BUFFER_SIZE];
	struct sockaddr_storage from_addr;
	socklen_t from_addr_len;
	
	from_addr_len = (socklen_t) sizeof(from_addr);
	memset((void *) &from_addr, 0, sizeof(from_addr));
	recv_len = recvfrom(dplpmtud_socket, buf, BUFFER_SIZE, 0, (struct sockaddr *) &from_addr, &from_addr_len);
	if (recv_len < 0) {
		LOG_PERROR("error receiving on dplpmtud socket.");
	} else {
		dplpmtud_message_handler(dplpmtud_socket, buf, recv_len, (struct sockaddr *)&from_addr, from_addr_len);
	}
	
	LOG_TRACE_LEAVE
}

static void *controller(void *arg) {
	LOG_TRACE_ENTER
	int icmp_socket;
	
	init_cblib();
	
	register_fd_callback(dplpmtud_socket, &dplpmtud_socket_readable, NULL);
	if (!dplpmtud_passive_mode) {
		
		if (dplpmtud_start_prober(dplpmtud_socket) < 0) {
			dplpmtud_passive_mode = 1;
			LOG_ERROR("Could not start prober");
		} else if (dplpmtud_handle_ptb) {
			icmp_socket = dplpmtud_ptb_handler_init(dplpmtud_socket);
			if (icmp_socket < 0) {
				LOG_ERROR("Could not start ptb listener");
			} else {
				register_fd_callback(icmp_socket, &dplpmtud_icmp_socket_readable, NULL);
			}
		}
	}
	
	// release super user privilege
	if (setuid(getuid()) != 0) {
		LOG_DEBUG("could not release super user privilege.");
	}
	handle_events();
	
	LOG_TRACE_LEAVE
	return 0;
}

pthread_t dplpmtud_start(int socket, int address_family, int passive_mode, int handle_ptb) {
	LOG_TRACE_ENTER
	if (dplpmtud_thread_id != 0) {
		// dplpmtud thread already started
		LOG_INFO("dplpmtud thread already started");
		LOG_TRACE_LEAVE
		return 0;
	}
	if (address_family == AF_INET) {
		dplpmtud_ip_version = IPv4;
	} else if (address_family == AF_INET6) {
		dplpmtud_ip_version = IPv6;
	} else {
		LOG_ERROR("unknown address family");
		LOG_TRACE_LEAVE
		return 0;
	}
	dplpmtud_socket = socket;
	
	dplpmtud_passive_mode = passive_mode;
	if (passive_mode) {
		dplpmtud_handle_ptb = 0;
	} else {
		dplpmtud_handle_ptb = handle_ptb;
	}
	
	pthread_create(&dplpmtud_thread_id, NULL, controller, NULL);
	LOG_TRACE_LEAVE
	return dplpmtud_thread_id;
}

void dplpmtud_wait() {
	LOG_TRACE_ENTER
	if (dplpmtud_thread_id != 0) {
		pthread_join(dplpmtud_thread_id, NULL);
	}
	LOG_TRACE_LEAVE
}
