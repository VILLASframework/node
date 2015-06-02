/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/timerfd.h>
#include <sys/syscall.h>

#include "utils.h"
#include "path.h"
#include "socket.h"
#include "timing.h"
#include "config.h"

#ifndef sigev_notify_thread_id
 #define sigev_notify_thread_id   _sigev_un._tid
#endif

extern struct settings settings;

static void path_write(struct path *p)
{
	FOREACH(&p->destinations, it) {
		int sent = node_write(
			it->node,			/* Destination node */
			p->pool,			/* Pool of received messages */
			p->poolsize,			/* Size of the pool */
			p->received-it->node->combine,	/* Index of the first message which should be sent */
			it->node->combine		/* Number of messages which should be sent */
		);

		debug(1, "Sent %u  messages to node '%s'", sent, it->node->name);
		p->sent += sent;
	}
}

/** Send messages asynchronously */
static void * path_run_async(void *arg)
{
	struct path *p = arg;
	struct itimerspec its = {
		.it_interval = time_from_double(1 / p->rate),
		.it_value = { 1, 0 }
	};

	p->tfd = timerfd_create(CLOCK_REALTIME, 0);
	if (p->tfd < 0)
		serror("Failed to create timer");

	if (timerfd_settime(p->tfd, 0, &its, NULL))
		serror("Failed to start timer");

	/* Block until 1/p->rate seconds elapsed */
	while (timerfd_wait(p->tfd))	
		path_write(p);

	return NULL;
}

/** Receive messages */
static void * path_run(void *arg)
{
	struct path *p = arg;
	
	/* Allocate memory for message pool */
	p->pool = alloc(p->poolsize * sizeof(struct msg));
	p->previous = p->current = p->pool;

	/* Open deferred TCP connection
	   TODO: fix this
	node_start_defer(p->in);

	FOREACH(&p->destinations, it)
		node_start_defer(it->node); */

	/* Main thread loop */
skip:	for(;;) {
		/* Receive message */
		int recv = node_read(p->in, p->pool, p->poolsize, p->received, p->in->combine);
		
		debug(10, "Received %u messages from node '%s'", recv, p->in->name);
		
		/* For each received message... */
		for (int i=0; i<recv; i++) {
			p->previous = &p->pool[(p->received-1) % p->poolsize];
			p->current  = &p->pool[ p->received    % p->poolsize];
			
			p->received++;
			
			/* Check header fields */
			if (msg_verify(p->current)) {
				p->invalid++;
				goto skip; /* Drop message */
			}

			/* Update histogram and handle wrap-around of sequence number */
			int dist = (UINT16_MAX + p->current->sequence - p->previous->sequence) % UINT16_MAX;
			if (dist > UINT16_MAX / 2)
				dist -= UINT16_MAX;

			hist_put(&p->histogram, dist);

			/* Handle simulation restart */
			if (p->current->sequence == 0 && abs(dist) >= 1) {
				char buf[33];
				path_print(p, buf, sizeof(buf));
				warn("Simulation for path %s restarted (prev->seq=%u, current->seq=%u, dist=%d)",
					buf, p->previous->sequence, p->current->sequence, dist);

				path_reset(p);
			}
			else if (dist <= 0 && p->received > 1) {
				p->dropped++;
				goto skip;
			}
		}
		
		/* Call hook callbacks */
		FOREACH(&p->hooks, it) {
			if (it->hook(p->current, p)) {
				p->skipped++;
				goto skip;
			}
		}
		
		/* At fixed rate mode, messages are send by another thread */
		if (!p->rate)
			path_write(p);
	}

	return NULL;
}

int path_start(struct path *p)
{ INDENT
	char buf[33];
	path_print(p, buf, sizeof(buf));
	
	info("Starting path: %s (poolsize = %u)", buf, p->poolsize);

	/* At fixed rate mode, we start another thread for sending */
	if (p->rate)
		pthread_create(&p->sent_tid, NULL, &path_run_async, p);

	return  pthread_create(&p->recv_tid, NULL, &path_run,  p);
}

int path_stop(struct path *p)
{ INDENT
	char buf[33];
	path_print(p, buf, sizeof(buf));
	
	info("Stopping path: %s", buf);

	pthread_cancel(p->recv_tid);
	pthread_join(p->recv_tid, NULL);

	if (p->rate) {
		pthread_cancel(p->sent_tid);
		pthread_join(p->sent_tid, NULL);

		close(p->tfd);
	}

	if (p->received)
		hist_print(&p->histogram);

	return 0;
}

int path_reset(struct path *p)
{
	p->sent		= 0;
	p->received	= 1;
	p->invalid	= 0;
	p->skipped	= 0;
	p->dropped	= 0;

	hist_reset(&p->histogram);
	
	return 0;
}

void path_print_stats(struct path *p)
{
	char buf[33];
	path_print(p, buf, sizeof(buf));
	
	info("%-32s :   %-8u %-8u %-8u %-8u %-8u", buf,
		p->sent, p->received, p->dropped, p->skipped, p->invalid);
}

int path_print(struct path *p, char *buf, int len)
{
	*buf = 0;
	
	strap(buf, len, "%s " MAG("=>"), p->in->name);
		
	if (list_length(&p->destinations) > 1) {
		strap(buf, len, " [");
		FOREACH(&p->destinations, it)
			strap(buf, len, " %s", it->node->name);
		strap(buf, len, " ]");
	}
	else
		strap(buf, len, " %s", p->out->name);
	
	return 0;
}

struct path * path_create()
{
	struct path *p = alloc(sizeof(struct path));

	list_init(&p->destinations, NULL);
	list_init(&p->hooks, NULL);

	hist_create(&p->histogram, -HIST_SEQ, +HIST_SEQ, 1);

	return p;
}

void path_destroy(struct path *p)
{
	list_destroy(&p->destinations);
	list_destroy(&p->hooks);
	hist_destroy(&p->histogram);
	
	free(p->pool);
	free(p);
}
