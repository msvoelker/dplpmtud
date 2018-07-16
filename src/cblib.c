/*-
 * Copyright (c) 2004-2011, Michael Tuexen.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Version 1.0 Initial release
 * Version 1.1 Bug fix from Ingmar Mohring
 * Versino 1.2 Add deregister routines from Philipp Kirchner
 * Version 1.3 Bug fix from Thomas Dreibholz
 * Version 1.4 Bug fix in stop_timer.
 * Version 1.5 Bug fix in handling fd. Don't use the fd as the index.
 * Version 1.6 Bug fix in the first argument of select()
 */
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include "logger.h"
#if LOG_LEVEL <= LOG_LEVEL_DEBUG
#define DEBUG 1
#endif
#ifdef DEBUG
#include <stdio.h>
#endif

#include "cblib.h"

static struct cbarg {
	int fd;
	void *arg;
	void (*callback) (void *);
} cb[FD_SETSIZE];

static struct timer *timer_list = NULL;

static fd_set read_fds;
static int maxfd;

static void *
dummy (void *arg)
{
	int fd;

	fd = *(int *)arg;
#ifdef DEBUG
	fprintf(stderr, "This function should not have been called. Clearing fd = %u\n", fd);
#endif
	if (fd >= 0) {
		FD_CLR(fd, &read_fds);
	}
	return(arg);
}

void
init_cblib(void)
{
	unsigned int i;

	FD_ZERO(&read_fds);
	maxfd = -1;
	for (i = 0; i < FD_SETSIZE; i++) {
		cb[i].fd = -1;
		cb[i].callback = (void (*)(void *))dummy;
		cb[i].arg = (void *) (&cb[i].fd);
	}
}

void
register_fd_callback(int fd, void (*callback)(void *), void *arg)
{
	unsigned int i;

	if ((fd <= 0) || (callback == NULL)) {
#ifdef DEBUG
		fprintf(stderr, "register_fd_callback: invalid arguments.\n");
#endif
		return;
	}
	for (i = 0; i < FD_SETSIZE; i++) {
		if (cb[i].fd == fd) {
			break;
		}
	}
	if (i < FD_SETSIZE) {
#ifdef DEBUG
		fprintf(stderr, "register_fd_callback: file descriptor %d already registered.\n", fd);
#endif
		cb[i].callback = callback;
		cb[i].arg = arg;
		return;
	}
	for (i = 0; i < FD_SETSIZE; i++) {
		if (cb[i].fd == -1) {
			break;
		}
	}
	if (i < FD_SETSIZE) {
		cb[i].fd = fd;
		cb[i].callback = callback;
		cb[i].arg = arg;
		FD_SET(fd, &read_fds);
		if (fd > maxfd) {
			maxfd = fd;
		}
	} else {
#ifdef DEBUG
		fprintf(stderr, "register_fd_callback: file descriptor table full.\n");
#endif
	}
}

void
deregister_fd_callback(int fd)
{
	unsigned int i;

	if (fd <= 0) {
#ifdef DEBUG
		fprintf(stderr, "deregister_fd_callback: invalid arguments.\n");
#endif
		return;
	}
	for (i = 0; i < FD_SETSIZE; i++) {
		if (cb[i].fd == fd) {
			break;
		}
	}
	if (i < FD_SETSIZE) {
		cb[i].fd = -1;
		cb[i].callback = (void (*)(void *))dummy;
		cb[i].arg = (void *) (&cb[i].fd);
		if (maxfd == fd) {
			maxfd = -1;
			for (i = 0; i < FD_SETSIZE; i++) {
				if (cb[i].fd > maxfd) {
					maxfd = cb[i].fd;
				}
			}
		}
	} else {
#ifdef DEBUG
		fprintf(stderr, "deregister_fd_callback: file descriptor not found.\n");
#endif
	}
	if (FD_ISSET(fd, &read_fds)){
		FD_CLR(fd, &read_fds);
	}
}

void
register_stdin_callback(void (*callback)(void *), void *arg)
{
	unsigned int i;

	if (callback == NULL) {
#ifdef DEBUG
		fprintf(stderr, "register_stdin_callback: invalid arguments.\n");
#endif
		return;
	}
	for (i = 0; i < FD_SETSIZE; i++) {
		if (cb[i].fd == 0) {
			break;
		}
	}
	if (i < FD_SETSIZE) {
#ifdef DEBUG
		fprintf(stderr, "register_stdin_callback: file descriptor already registered.\n");
#endif
		cb[i].callback = callback;
		cb[i].arg = arg;
		return;
	}
	for (i = 0; i < FD_SETSIZE; i++) {
		if (cb[i].fd == -1) {
			break;
		}
	}
	if (i < FD_SETSIZE) {
		cb[i].fd = 0;
		cb[i].callback = callback;
		cb[i].arg = arg;
		FD_SET(0, &read_fds);
		if (maxfd < 0) {
			maxfd = 0;
		}
	} else {
#ifdef DEBUG
		fprintf(stderr, "register_stdin_callback: file descriptor table full.\n");
#endif
	}
}

void
deregister_stdin_callback(void)
{
	unsigned int i;

	for (i = 0; i < FD_SETSIZE; i++) {
		if (cb[i].fd == 0) {
			break;
		}
	}
	if (i < FD_SETSIZE) {
		cb[i].fd = -1;
		cb[i].callback = (void (*)(void *))dummy;
		cb[i].arg = (void *) (&cb[i].fd);
		if (maxfd == 0) {
			maxfd = -1;
		}
	} else {
#ifdef DEBUG
		fprintf(stderr, "deregister_fd_callback: file descriptor not found.\n");
#endif
	}
	if (FD_ISSET(0, &read_fds)){
		FD_CLR(0, &read_fds);
	}
}

struct timer *
create_timer(void (*callback)(void *), void *arg, char *description)
{
	struct timer *timer;

	if ((callback == NULL) || (description == NULL)) {
#ifdef DEBUG
		fprintf(stderr, "create_timer was called with a NULL argument.\n");
#endif
		return NULL;
	}
	timer = (struct timer *)malloc(sizeof(struct timer));
	if (timer) {
		memset((void *)timer, 0, sizeof(struct timer));
		strncpy(timer->description, description, MAX_DESCRIPTION_LENGTH);
		timer->callback = callback;
		timer->arg = arg;
	} else {
#ifdef DEBUG
		fprintf(stderr, "create_timer could not allocate memory.\n");
#endif
	}
	return timer;
}

void
delete_timer(struct timer *timer)
{
#ifdef DEBUG
	if (timer == NULL) {
		fprintf(stderr, "delete_timer called with NULL argument.\n");
	} else if ((timer->next != NULL) ||
	           (timer->prev != NULL) ||
	           (timer->start_time.tv_sec != 0) ||
	           (timer->start_time.tv_usec != 0) ||
	           (timer->stop_time.tv_sec != 0) ||
	           (timer->stop_time.tv_usec != 0)) {
		fprintf(stderr, "Timer %.*s not stoped.\n", MAX_DESCRIPTION_LENGTH, timer->description);
	}
#endif
	if (timer != NULL) {
		free((void *)timer);
	}
}

static int
timeval_before(struct timeval *tv1, struct timeval *tv2)
{
	if ((tv1 == NULL) || tv2 == NULL) {
#ifdef DEBUG
		fprintf(stderr, "timer_val before called with NULL argument.\n");
#endif
		return(1);
	}
	if ((tv1->tv_sec < tv2->tv_sec) || ((tv1->tv_sec == tv2->tv_sec) && (tv1->tv_usec < tv2->tv_usec)))
		return(1);
	else
		return(0);
}

static void
get_time(struct timeval *tv)
{
	memset((void *)tv, 0, sizeof(struct timeval));
	if (gettimeofday(tv, NULL) < 0) {
#ifdef DEBUG
		fprintf(stderr, "get_time could not figure out the current time.\n");
#endif
	}
}

void
start_timer(struct timer *timer_to_start, unsigned int timeout)
{
	struct timeval now;
	struct timer *timer;

	get_time(&now);

	if (timer_to_start != NULL) {
		/* check if timer_to_start is not aready started yet. */
		if ((timer_to_start->next != NULL) ||
		    (timer_to_start->prev != NULL) ||
		    (timer_to_start->start_time.tv_sec != 0) ||
		    (timer_to_start->start_time.tv_usec != 0) ||
		    (timer_to_start->stop_time.tv_sec != 0) ||
		    (timer_to_start->stop_time.tv_usec != 0)) {
#ifdef DEBUG
			fprintf(stderr, "Timer %.*s is already started.\n", MAX_DESCRIPTION_LENGTH, timer_to_start->description);
#endif
			return;
		}
		/* set start_time and stop_time */
		timer_to_start->start_time.tv_sec = now.tv_sec;
		timer_to_start->start_time.tv_usec = now.tv_usec;
		timer_to_start->stop_time.tv_sec = now.tv_sec + (timeout / 1000);
		timer_to_start->stop_time.tv_usec = now.tv_usec + 1000 * (timeout % 1000);
		if (timer_to_start->stop_time.tv_usec >= 1000000) {
			timer_to_start->stop_time.tv_sec++;
			timer_to_start->stop_time.tv_usec -= 1000000;
		}

		if (timer_list == NULL) {
			timer_list = timer_to_start;
			return;
		}

		timer = timer_list;
		if (timeval_before(&timer_to_start->stop_time, &timer->stop_time)) {
			timer_to_start->next = timer;
			timer_to_start->prev = NULL;
			timer->prev = timer_to_start;
			timer_list = timer_to_start;
			return;
		}

		for (timer = timer_list; timer->next; timer = timer->next) {
			if (timeval_before(&timer_to_start->stop_time, &timer->next->stop_time)) {
				break;
			}
		}
		if (timer->next) {
			timer->next->prev = timer_to_start;
		}
		timer_to_start->next = timer->next;
		timer->next = timer_to_start;
		timer_to_start->prev = timer;
		return;
	} else {
#ifdef DEBUG
		fprintf(stderr, "start_timer called with NULL argument.\n");
#endif
		return;
	}
}

void
stop_timer(struct timer *timer_to_stop)
{
	struct timer *timer;

	for (timer = timer_list; timer; timer = timer->next) {
		if (timer == timer_to_stop) {
			break;
		}
	}
	if (timer != NULL) {
		if (timer->prev) {
			timer->prev->next = timer->next;
		}
		if (timer->next) {
			timer->next->prev = timer->prev;
		}
		if (timer_list == timer) {
			timer_list = timer->next;
		}
	} else {
#ifdef DEBUG
		fprintf(stderr, "stop_timer did not found timer in the list.\n");
#endif
	}

	if (timer_to_stop != NULL) {
		timer_to_stop->prev = NULL;
		timer_to_stop->next = NULL;
		timer_to_stop->start_time.tv_sec = 0;
		timer_to_stop->start_time.tv_usec = 0;
		timer_to_stop->stop_time.tv_sec = 0;
		timer_to_stop->stop_time.tv_usec = 0;
	} else {
#ifdef DEBUG
		fprintf(stderr, "stop_timer called with NULL argument.\n");
#endif
	}
}

#ifdef DEBUG
void
print_timer(struct timer *timer)
{
	fprintf(stderr, "Timer %.*s (stop_time = %u:%u).\n", MAX_DESCRIPTION_LENGTH, timer->description, (unsigned int)timer->stop_time.tv_sec, (unsigned int)timer->stop_time.tv_usec);
}

void
print_timers(void)
{
	struct timer *timer;

	for (timer = timer_list; timer; timer = timer->next) {
		print_timer(timer);
	}
}
#endif

static void
handle_event(void)
{
	int n;
	unsigned int i;
	struct timer *timer;
	struct timeval now;
	struct timeval timeout;
	struct timeval *timeout_ptr;
	fd_set rset;

	get_time(&now);
#ifdef DEBUG
	fprintf(stderr, "handle_event called at time %u:%u.\n", (unsigned int)now.tv_sec, (unsigned int)now.tv_usec);
	print_timers();
#endif
	timer = timer_list;
	if ((timer != NULL) && (timeval_before(&timer->stop_time, &now))) {
#ifdef DEBUG
		fprintf(stderr, "Handling timer event (%.*s) which should have already be handled.\n", MAX_DESCRIPTION_LENGTH, timer->description);
#endif
		stop_timer(timer);
		(*timer->callback)(timer->arg);
		return;
	}

	if (timer != NULL) {
		timeout.tv_sec = timer->stop_time.tv_sec - now.tv_sec;
		timeout.tv_usec = timer->stop_time.tv_usec - now.tv_usec;
		if (timeout.tv_usec < 0) {
			timeout.tv_sec--;
			timeout.tv_usec += 1000000;
		}
		timeout_ptr = &timeout;
	} else {
		timeout_ptr = NULL;
	}

	rset = read_fds;
	n = select(maxfd + 1, &rset, NULL, NULL, timeout_ptr);
#ifdef DEBUG
	get_time(&now);
	fprintf(stderr, "handle_event reacts at time %u:%u.\n", (unsigned int)now.tv_sec, (unsigned int)now.tv_usec);
#endif

	if (n == 0) {
		stop_timer(timer);
#ifdef DEBUG
		fprintf(stderr, "Handling timer event (%.*s).\n", MAX_DESCRIPTION_LENGTH, timer->description);
#endif
		(*timer->callback)(timer->arg);
	} else {
		for (i = 0; i < FD_SETSIZE; i++) {
			if ((cb[i].fd >= 0) && FD_ISSET(cb[i].fd, &rset)) {
#ifdef DEBUG
				fprintf(stderr, "Handling fd event for fd = %d.\n", cb[i].fd);
#endif
				(*(cb[i].callback))(cb[i].arg);
			}
		}
	}
	return;
}

void
handle_events(void)
{
	while (1) {
		handle_event();
	}
}
