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

#include <sys/time.h>

#define MAX_DESCRIPTION_LENGTH 100

struct timer {
	struct timer *prev;
	struct timer *next;
	struct timeval start_time;
	struct timeval stop_time;
	char description[MAX_DESCRIPTION_LENGTH + 1];
	void *arg;
	void (*callback) (void *);
};

struct timer *
create_timer(void (*callback) (void *), void *arg, char *description);

void
delete_timer(struct timer *timer);

void
start_timer(struct timer *timer, unsigned int timeout);

void
stop_timer(struct timer *timer);

void
handle_events(void);

void
init_cblib(void);

void
register_fd_callback(int fd, void (*callback)(void *), void *arg);

void
deregister_fd_callback(int fd);

void
register_stdin_callback(void (*callback)(void *), void *arg);

void
deregister_stdin_callback(void);

#ifdef DEBUG
void
print_timer(struct timer *timer);

void
print_timers(void);
#endif
