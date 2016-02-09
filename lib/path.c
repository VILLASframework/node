/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <unistd.h>
#include <inttypes.h>

#include "utils.h"
#include "path.h"
#include "timing.h"
#include "config.h"
#include "pool.h"

static void path_write(struct path *p)
{
	list_foreach(struct node *n, &p->destinations) {
		int sent = node_write(
			n,			/* Destination node */
			&p->pool,		/* Pool of received messages */
			n->vectorize		/* Number of messages which should be sent */
		);

		debug(15, "Sent %u messages to node %s", sent, node_name(n));
		p->sent += sent;

		p->ts.sent = time_now(); /** @todo use hardware timestamps for socket node type */
	}
}

/** Send messages asynchronously */
static void * path_run_async(void *arg)
{
	struct path *p = arg;

	/* Block until 1/p->rate seconds elapsed */
	for (;;) {
		/* Check for overruns */
		uint64_t expir = timerfd_wait(p->tfd);
		if (expir == 0)
			perror("Failed to wait for timer");
		else if (expir > 1) {
			p->overrun += expir;
			warn("Overrun detected for path: overruns=%" PRIu64, expir);
		}
		
		if (p->received == 0)
			continue;

		if (hook_run(p, HOOK_ASYNC))
			continue;

		path_write(p);
	}

	return NULL;
}

/** Receive messages */
static void * path_run(void *arg)
{
	struct path *p = arg;

	/* Main thread loop */
	for (;;) {
		/* Receive message */
		int recv = node_read(p->in, &p->pool, p->in->vectorize);
		if (recv < 0)
			error("Failed to receive message from node %s", node_name(p->in));
		else if (recv == 0)
			continue;

		/** @todo Replace this timestamp by hardware timestamping for node type which support it. */
		p->ts.last = p->ts.recv;
		p->ts.recv = time_now();
			
		debug(15, "Received %u messages from node %s", recv, node_name(p->in));

		/* Run preprocessing hooks */
		if (hook_run(p, HOOK_PRE)) {
			p->skipped += recv;
			continue;
		}

		/* For each received message... */
		for (int i = 0; i < recv; i++) {
			/* Update tail pointer of message pool by the amount of actually received messages. */
			pool_push(&p->pool, 1);
			
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

		/* At fixed rate mode, messages are send by another (asynchronous) thread */
		if (!p->rate)
			path_write(p);
	}

	return NULL;
}

int path_start(struct path *p)
{
	info("Starting path: %s (poollen=%zu, msgsize=%zu, #hooks=%zu, rate=%.1f)",
		path_name(p), pool_length(&p->pool), pool_stride(&p->pool), list_length(&p->hooks), p->rate);
	
	/* We sort the hooks according to their priority before starting the path */
	list_sort(&p->hooks, hooks_sort_priority);

	if (hook_run(p, HOOK_PATH_START))
		return -1;

	/* At fixed rate mode, we start another thread for sending */
	if (p->rate) {
		p->tfd = timerfd_create_rate(p->rate);
		if (p->tfd < 0)
			serror("Failed to create timer");

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

	if (hook_run(p, HOOK_PATH_STOP))
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

struct path * path_create(size_t poolsize, size_t values)
{
	struct path *p = alloc(sizeof(struct path));

	list_init(&p->destinations, NULL);
	list_init(&p->hooks, free);

	pool_create(&p->pool, poolsize, 16 + values * sizeof(float)); /** @todo */

	list_foreach(struct hook *h, &hooks) {
		if (h->type & HOOK_INTERNAL)
			list_push(&p->hooks, memdup(h, sizeof(*h)));
	}
	
	p->state = PATH_CREATED;

	return p;
}

void path_destroy(struct path *p)
{	
	list_destroy(&p->destinations);
	list_destroy(&p->hooks);
	pool_destroy(&p->pool);

	free(p->_name);
	free(p);
}

int path_uses_node(struct path *p, struct node *n) {
	return (p->in == n) || list_contains(&p->destinations, n) ? 0 : 1;
}
