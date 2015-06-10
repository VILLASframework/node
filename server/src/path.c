/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#include <stdlib.h>
#include <unistd.h>

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
		
		clock_gettime(CLOCK_REALTIME, &p->ts_sent);
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

	/* Main thread loop */
	for(;;) {
		/* Receive message */
		int recv = node_read(p->in, p->pool, p->poolsize, p->received, p->in->combine);
		
		/** @todo Replace this timestamp by hardware timestamping */
		clock_gettime(CLOCK_REALTIME, &p->ts_recv);
		
		debug(10, "Received %u messages from node '%s'", recv, p->in->name);
		
		/* Run preprocessing hooks */
		if (hook_run(p, HOOK_PRE)) {
			p->skipped += recv;
			continue;
		}
		
		/* For each received message... */
		for (int i = 0; i < recv; i++) {
			p->previous = &p->pool[(p->received-1) % p->poolsize];
			p->current  = &p->pool[ p->received    % p->poolsize];
			
			if (settings.debug >= 10)
				msg_fprint(stdout, p->current);
			
			p->received++;
			
			/* Run hooks for filtering, stats collection and manipulation */
			if (hook_run(p, HOOK_MSG)) {
				p->skipped++;
				continue;
			}
		}
		
		/* Run post processing hooks */
		if (hook_run(p, HOOK_POST)) {
			p->skipped += recv;
			continue;
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
	
	if (hook_run(p, HOOK_PATH_START))
		return -1;

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
	
	if (hook_run(p, HOOK_PATH_STOP))
		return -1;

	return 0;
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

int path_reset(struct path *p)
{
	if (hook_run(p, HOOK_PATH_RESTART))
		return -1;

	p->sent	    =
	p->received =
	p->invalid  =
	p->skipped  =
	p->dropped  = 0;
		
	return 0;
}

struct path * path_create()
{
	struct path *p = alloc(sizeof(struct path));

	list_init(&p->destinations, NULL);
	
	for (int i = 0; i < HOOK_MAX; i++)
		list_init(&p->hooks[i], NULL);

#define hook_add(type, priority, cb) list_insert(&p->hooks[type], priority, cb)
	
	hook_add(HOOK_MSG,		1,	hook_verify);
	hook_add(HOOK_MSG,		2,	hook_restart);
	hook_add(HOOK_MSG,		3,	hook_drop);

	return p;
}

void path_destroy(struct path *p)
{
	list_destroy(&p->destinations);
	
	for (int i = 0; i < HOOK_MAX; i++)
		list_destroy(&p->hooks[i]);
		
	free(p->pool);
	free(p);
}
