/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "utils.h"
#include "path.h"
#include "timing.h"
#include "config.h"

static void path_write(struct path *p)
{
	list_foreach(struct node *n, &p->destinations) {
		int sent = node_write(
			n,			/* Destination node */
			p->pool,		/* Pool of received messages */
			p->poolsize,		/* Size of the pool */
			p->received - n->combine,/* Index of the first message which should be sent */
			n->combine		/* Number of messages which should be sent */
		);

		debug(15, "Sent %u messages to node %s", sent, node_name(n));
		p->sent += sent;

		p->ts.sent = time_now(); /** @todo use hardware timestamps for socket node type */
	}
}

int path_run_hook(struct path *p, enum hook_type t)
{
	int ret = 0;

	list_foreach(struct hook *h, &p->hooks) {
		if (h->type & t) {
			ret = ((hook_cb_t) h->cb)(p, h, t);
			debug(22, "Running hook when=%u '%s' prio=%u ret=%d", t, h->name, h->priority, ret);
			if (ret)
				return ret;
		}
	}

	return ret;
}

/** Send messages asynchronously */
static void * path_run_async(void *arg)
{
	struct path *p = arg;

	/* Block until 1/p->rate seconds elapsed */
	for (;;) {
		/* Check for overruns */
		uint64_t expir = timerfd_wait(p->tfd);
		if (expir > 1) {
			p->overrun += expir;
			warn("Overrun detected for path: overruns=%" PRIu64, expir);
		}
		
		if (p->received == 0)
			continue;

		if (path_run_hook(p, HOOK_ASYNC))
			continue;

		path_write(p);
	}

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
	for (;;) {
		/* Receive message */
		int recv = node_read(p->in, p->pool, p->poolsize, p->received, p->in->combine);
		if (recv < 0)
			error("Failed to receive message from node %s", node_name(p->in));
		else if (recv == 0)
			continue;

		/** @todo Replace this timestamp by hardware timestamping for node type which support it. */
		p->ts.last = p->ts.recv;
		p->ts.recv = time_now();
			
		debug(15, "Received %u messages from node %s", recv, node_name(p->in));

		/* Run preprocessing hooks */
		if (path_run_hook(p, HOOK_PRE)) {
			p->skipped += recv;
			continue;
		}

		/* For each received message... */
		for (int i = 0; i < recv; i++) {
			p->previous = p->current;
			p->current  = &p->pool[p->received % p->poolsize];

			p->received++;

			/* Run hooks for filtering, stats collection and manipulation */
			if (path_run_hook(p, HOOK_MSG)) {
				p->skipped++;
				continue;
			}
		}

		/* Run post processing hooks */
		if (path_run_hook(p, HOOK_POST)) {
			p->skipped += recv;
			continue;
		}

		/* At fixed rate mode, messages are send by another (asynchronous) thread */
		if (!p->rate)
			path_write(p);
	}

	return NULL;
}

int path_start(struct path *p)
{
	info("Starting path: %s (poolsize=%u, msgsize=%u, #hooks=%zu, rate=%.1f)",
		path_name(p), p->poolsize, p->msgsize, list_length(&p->hooks), p->rate);
	
	/* We sort the hooks according to their priority before starting the path */
	list_sort(&p->hooks, hooks_sort_priority);

	if (path_run_hook(p, HOOK_PATH_START))
		return -1;

	/* At fixed rate mode, we start another thread for sending */
	if (p->rate) {
		struct itimerspec its = {
			.it_interval = time_from_double(1 / p->rate),
			.it_value = { 0, 1 }
		};

		p->tfd = timerfd_create(CLOCK_REALTIME, 0);
		if (p->tfd < 0)
			serror("Failed to create timer");

		if (timerfd_settime(p->tfd, 0, &its, NULL))
			serror("Failed to start timer");

		pthread_create(&p->sent_tid, NULL, &path_run_async, p);
	}
	
	p->state = PATH_RUNNING;

	return pthread_create(&p->recv_tid, NULL, &path_run,  p);
}

int path_stop(struct path *p)
{
	info("Stopping path: %s", path_name(p));

	pthread_cancel(p->recv_tid);
	pthread_join(p->recv_tid, NULL);

	if (p->rate) {
		pthread_cancel(p->sent_tid);
		pthread_join(p->sent_tid, NULL);

		close(p->tfd);
	}
	
	p->state = PATH_STOPPED;

	if (path_run_hook(p, HOOK_PATH_STOP))
		return -1;

	return 0;
}

const char * path_name(struct path *p)
{
	if (!p->_name) {	
		strcatf(&p->_name, "%s " MAG("=>"), p->in->name);

		list_foreach(struct node *n, &p->destinations) {
			strcatf(&p->_name, " %s", n->name);
		}
	}

	return p->_name;
}

struct path * path_create()
{
	struct path *p = alloc(sizeof(struct path));

	list_init(&p->destinations, NULL);
	list_init(&p->hooks, free);

	list_foreach(struct hook *h, &hooks) {
		if (h->type & HOOK_INTERNAL)
			list_push(&p->hooks, memdup(h, sizeof(*h)));
	}
	
	p->state = PATH_CREATED;

	return p;
}

void path_destroy(struct path *p)
{
	path_run_hook(p, HOOK_DEINIT);
	
	list_destroy(&p->destinations);
	list_destroy(&p->hooks);

	free(p->_name);
	free(p->pool);
	free(p);
}

int path_uses_node(struct path *p, struct node *n) {
	return (p->in == n) || list_contains(&p->destinations, n) ? 0 : 1;
}
