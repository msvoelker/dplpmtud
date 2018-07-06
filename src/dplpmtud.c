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

#define BUFFER_SIZE (1<<16)

int dplpmtud_socket = 0;

static pthread_t dplpmtud_thread_id = 0;
static int dplpmtud_passive_mode;
static int dplpmtud_handle_ptb;

void dplpmtud_socket_readable(void *arg) {
	LOG_DEBUG("dplpmtud_socket_readable entered");
	ssize_t recv_len;
	char buf[BUFFER_SIZE];
	struct sockaddr_storage from_addr;
	socklen_t from_addr_len;
	
	from_addr_len = (socklen_t) sizeof(from_addr);
	memset((void *) &from_addr, 0, sizeof(from_addr));
	recv_len = recvfrom(dplpmtud_socket, buf, BUFFER_SIZE, 0, (struct sockaddr *) &from_addr, &from_addr_len);
	dplpmtud_message_handler(dplpmtud_socket, buf, recv_len, (struct sockaddr *)&from_addr, from_addr_len);
	
	LOG_DEBUG("leave dplpmtud_socket_readable");
}

static void *controller(void *arg) {
	LOG_DEBUG("controller entered");
	int icmp_socket;
	
	init_cblib();
	
	register_fd_callback(dplpmtud_socket, &dplpmtud_socket_readable, NULL);
	if (!dplpmtud_passive_mode) {
		
		if (dplpmtud_handle_ptb) {
			icmp_socket = dplpmtud_ptb_handler_init(dplpmtud_socket);
			register_fd_callback(icmp_socket, &dplpmtud_icmp_socket_readable, NULL);
		}
		
		dplpmtud_start_prober(dplpmtud_socket);
	}
	
	// release super user privilege
	if (setuid(getuid()) != 0) {
		LOG_DEBUG("could not release super user privilege.");
	}
	handle_events();
	
	return 0;
}

pthread_t dplpmtud_start(int socket, int address_family, int passive_mode, int handle_ptb) {
	LOG_DEBUG("dplpmtud_start entered");
	if (dplpmtud_thread_id != 0) {
		// dplpmtud thread already started
		LOG_INFO("dplpmtud thread already started");
		LOG_DEBUG("leave dplpmtud_start");
		return 0;
	}
	if (address_family == AF_INET) {
		dplpmtud_ip_version = IPv4;
	} else if (address_family == AF_INET6) {
		dplpmtud_ip_version = IPv6;
	} else {
		LOG_ERROR("unknown address family");
		LOG_DEBUG("leave dplpmtud_start");
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
	LOG_DEBUG("leave dplpmtud_start");
	return dplpmtud_thread_id;
}

void dplpmtud_wait() {
	LOG_DEBUG("dplpmtud_wait entered");
	if (dplpmtud_thread_id != 0) {
		pthread_join(dplpmtud_thread_id, NULL);
	}
	LOG_DEBUG("leave dplpmtud_wait");
}
