/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>

#include "config.h"
#include "utils.h"
#include "path.h"
#include "timing.h"
#include "pool.h"
#include "queue.h"

static void path_write(struct path *p, bool resend)
{
	list_foreach(struct node *n, &p->destinations) {
		int cnt = n->vectorize;
		int sent, tosend, available, released;
		struct sample *smps[n->vectorize];

		available = queue_pull_many(&p->queue, (void **) smps, cnt);
		if (available < cnt)
			warn("Queue underrun for path %s: available=%u expected=%u", path_name(p), available, cnt);
		
		if (available == 0)
			continue;
		
		tosend = hook_run(p, smps, available, HOOK_WRITE);
		if (tosend == 0)
			continue;
		
		sent = node_write(n, smps, tosend);
		if (sent < 0)
			error("Failed to sent %u samples to node %s", cnt, node_name(n));
		else if (sent < tosend)
			warn("Partial write to node %s", node_name(n));

		debug(LOG_PATH | 15, "Sent %u messages to node %s", sent, node_name(n));

		released = pool_put_many(&p->pool, (void **) smps, sent);
		if (sent != released)
			warn("Failed to release %u samples to pool for path %s", sent - released, path_name(p));
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
		
		if (hook_run(p, NULL, 0, HOOK_ASYNC))
			continue;

		path_write(p, true);
	}

	return NULL;
}

/** Receive messages */
static void * path_run(void *arg)
{
	struct path *p = arg;
	unsigned cnt = p->in->vectorize;
	int recv, enqueue, enqueued;
	int ready = 0; /**< Number of blocks in smps[] which are allocated and ready to be used by node_read(). */
	struct sample *smps[cnt];

	/* Main thread loop */
	for (;;) {
		/* Fill smps[] free sample blocks from the pool */
		ready += sample_get_many(&p->pool, smps, cnt - ready);
		if (ready != cnt)
			warn("Pool underrun for path %s", path_name(p));

		/* Read ready samples and store them to blocks pointed by smps[] */
		recv = p->in->_vt->read(p->in, smps, ready);
		if (recv < 0)
			error("Failed to receive message from node %s", node_name(p->in));
		else if (recv < ready)
			warn("Partial read for path %s: read=%u expected=%u", path_name(p), recv, ready);

		debug(LOG_PATH | 15, "Received %u messages from node %s", recv, node_name(p->in));

		/* Run preprocessing hooks for vector of samples */
		enqueue = hook_run(p, smps, recv, HOOK_READ);
		if (enqueue != recv) {
			info("Hooks skipped %u out of %u samples for path %s", recv - enqueue, recv, path_name(p));
			p->skipped += recv - enqueue;
		}

		enqueued = queue_push_many(&p->queue, (void **) smps, enqueue);
		if (enqueue != enqueued)
			warn("Failed to enqueue %u samples for path %s", enqueue - enqueued, path_name(p));

		ready -= enqueued;

		debug(LOG_PATH | 3, "Enqueuing %u samples to queue of path %s", enqueue, path_name(p));

		/* At fixed rate mode, messages are send by another (asynchronous) thread */
		if (p->rate == 0)
			path_write(p, false);
	}

	return NULL;
}

int path_start(struct path *p)
{
	int ret;

	info("Starting path: %s (#hooks=%zu, rate=%.1f)",
		path_name(p), list_length(&p->hooks), p->rate);

	ret = hook_run(p, NULL, 0, HOOK_PATH_START);
	if (ret)
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

	if (hook_run(p, NULL, 0, HOOK_PATH_STOP))
		return -1;

	return 0;
}

const char * path_name(struct path *p)
{
	if (!p->_name) {	
		strcatf(&p->_name, "%s " MAG("=>"), node_name_short(p->in));

		list_foreach(struct node *n, &p->destinations)
			strcatf(&p->_name, " %s", node_name_short(n));
	}

	return p->_name;
}

void path_init(struct path *p)
{
	list_init(&p->destinations);
	list_init(&p->hooks);
	
	/* Initialize hook system */
	list_foreach(struct hook *h, &hooks) {
		if (h->type & HOOK_INTERNAL)
			list_push(&p->hooks, memdup(h, sizeof(*h)));
	}

	p->state = PATH_CREATED;
}

int path_prepare(struct path *p)
{
	int ret;
	
	/* We sort the hooks according to their priority before starting the path */
	list_sort(&p->hooks, hooks_sort_priority);

	/* Allocate hook private memory */
	ret = hook_run(p, NULL, 0, HOOK_INIT);
	if (ret)
		error("Failed to initialize hooks of path: %s", path_name(p));

	/* Parse hook arguments */
	ret = hook_run(p, NULL, 0, HOOK_PARSE);
	if (ret)
		error("Failed to parse arguments for hooks of path: %s", path_name(p));

	/* Initialize queue */
	ret = pool_init(&p->pool, SAMPLE_LEN(p->samplelen), p->queuelen, &memtype_hugepage);
	if (ret)
		error("Failed to allocate memory pool for path");
	
	ret = queue_init(&p->queue, p->queuelen, &memtype_hugepage);
	if (ret)
		error("Failed to initialize queue for path");

	return 0;
}

void path_destroy(struct path *p)
{
	hook_run(p, NULL, 0, HOOK_DEINIT); /* Release memory */
	
	list_destroy(&p->destinations, NULL, false);
	list_destroy(&p->hooks, NULL, true);
	
	queue_destroy(&p->queue);
	pool_destroy(&p->pool);

	free(p->_name);
}

int path_uses_node(struct path *p, struct node *n) {
	return (p->in == n) || list_contains(&p->destinations, n) ? 0 : 1;
}
