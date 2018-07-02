
#include <sys/socket.h>
#include <string.h>
#include "logger.h"
#include "dplpmtud_pl.h"
#include "dplpmtud_main.h"

#define BUFFER_SIZE (1<<16)

void *dplpmtud_listener(void *arg) {
	LOG_DEBUG_("%s - listener entered", THREAD_NAME);
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
	
	LOG_DEBUG_("%s leave listener", THREAD_NAME);
	return NULL;
}
