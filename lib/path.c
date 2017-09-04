/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <poll.h>

#include "config.h"
#include "utils.h"
#include "path.h"
#include "timing.h"
#include "pool.h"
#include "queue.h"
#include "hook.h"
#include "plugin.h"
#include "memory.h"
#include "stats.h"
#include "node.h"

static int path_source_init(struct path_source *ps)
{
	int ret;

	ret = pool_init(&ps->pool, MAX(DEFAULT_QUEUELEN, ps->node->vectorize), SAMPLE_LEN(ps->node->samplelen), &memtype_hugepage);
	if (ret)
		return ret;

	return 0;
}

static int path_source_destroy(struct path_source *ps)
{
	int ret;

	ret = pool_destroy(&ps->pool);
	if (ret)
		return ret;

	return 0;
}

static void path_source_read(struct path *p, struct path_source *ps)
{
	int ready, recv, mux, enqueue, enqueued;
	int cnt = ps->node->vectorize;

	struct sample *read_smps[cnt];
	struct sample *muxed_smps[cnt];

	/* Fill smps[] free sample blocks from the pool */
	ready = sample_alloc(&ps->pool, read_smps, cnt);
	if (ready != cnt)
		warn("Pool underrun for path source %s", node_name(ps->node));

	/* Read ready samples and store them to blocks pointed by smps[] */
	recv = node_read(ps->node, read_smps, ready);
	if (recv == 0)
		goto out2;
	else if (recv < 0)
		error("Failed to receive message from node %s", node_name(ps->node));
	else if (recv < ready)
		warn("Partial read for path %s: read=%u, expected=%u", path_name(p), recv, ready);

	/* Mux samples */
	mux = sample_alloc(&p->pool, muxed_smps, recv);
	if (mux != recv)
		warn("Pool underrun for path %s", path_name(p));

	for (int i = 0; i < mux; i++) {
		mapping_remap(&ps->mappings, p->last_sample, read_smps[i], NULL);

		p->last_sample->sequence = p->sequence++;

		sample_copy(muxed_smps[i], p->last_sample);
	}

	/* Run processing hooks */
	enqueue = hook_process_list(&p->hooks, muxed_smps, mux);
	if (enqueue == 0)
		goto out1;

	/* Keep track of the lowest index that wasn't enqueued;
	 * all following samples must be freed here */
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

		enqueued = queue_push_many(&pd->queue, (void **) muxed_smps, enqueue);
		if (enqueue != enqueued)
			warn("Queue overrun for path %s", path_name(p));

		/* Increase reference counter of these samples as they are now also owned by the queue. */
		sample_get_many(muxed_smps, enqueued);

		debug(LOG_PATH | 15, "Enqueued %u samples from %s to queue of %s", enqueued, node_name(ps->node), node_name(pd->node));
	}

out1:	sample_put_many(muxed_smps, recv);
out2:	sample_put_many(read_smps, ready);
}

static int path_destination_init(struct path_destination *pd, int queuelen)
{
	int ret;

	ret = queue_init(&pd->queue, queuelen, &memtype_hugepage);
	if (ret)
		return ret;

	return 0;
}

static int path_destination_destroy(struct path_destination *pd)
{
	int ret;

	ret = queue_destroy(&pd->queue);
	if (ret)
		return ret;

	return 0;
}

static void path_poll(struct path *p)
{
	int ret;

	ret = poll(p->reader.pfds, p->reader.nfds, -1);
	if (ret < 0)
		serror("Failed to poll");

	int updates = 0;
	for (int i = 0; i < p->reader.nfds; i++) {
		struct path_source *ps = list_at(&p->sources, i);

		if (p->reader.pfds[i].revents & POLLIN) {
			path_source_read(p, ps);
			updates++;
		}
	}
}

static void path_write(struct path *p)
{
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

		int cnt = pd->node->vectorize;
		int sent;
		int available;
		int released;

		struct sample *smps[cnt];

		/* As long as there are still samples in the queue */
		while (1) {
			available = queue_pull_many(&pd->queue, (void **) smps, cnt);
			if (available == 0)
				break;
			else if (available < cnt)
				debug(LOG_PATH | 5, "Queue underrun for path %s: available=%u expected=%u", path_name(p), available, cnt);

			debug(LOG_PATH | 15, "Dequeued %u samples from queue of node %s which is part of path %s", available, node_name(pd->node), path_name(p));

			sent = node_write(pd->node, smps, available);
			if (sent < 0)
				error("Failed to sent %u samples to node %s", cnt, node_name(pd->node));
			else if (sent < available)
				warn("Partial write to node %s: written=%d, expected=%d", node_name(pd->node), sent, available);

			released = sample_put_many(smps, sent);

			debug(LOG_PATH | 15, "Released %d samples back to memory pool", released);
		}
	}
}

/** Main thread function per path: receive -> sent messages */
static void * path_run(void *arg)
{
	struct path *p = arg;

	for (;;) {
		/* We only need to poll in case there is more than one source */
		path_poll(p);
		path_write(p);
	}

	return NULL;
}

int path_init(struct path *p)
{
	int ret;

	assert(p->state == STATE_DESTROYED);

	list_init(&p->hooks);
	list_init(&p->destinations);
	list_init(&p->sources);

	p->_name = NULL;

	/* Default values */
	p->reverse = 0;
	p->enabled = 1;
	p->queuelen = DEFAULT_QUEUELEN;

	/* Add internal hooks if they are not already in the list */
	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *q = list_at(&plugins, i);

		if (q->type != PLUGIN_TYPE_HOOK)
			continue;

		struct hook_type *vt = &q->hook;

		if ((vt->flags & HOOK_PATH) && (vt->flags & HOOK_BUILTIN)) {
			struct hook *h = alloc(sizeof(struct hook));

			ret = hook_init(h, vt, p, NULL);
			if (ret)
				return ret;

			list_push(&p->hooks, h);
		}
	}

	p->state = STATE_INITIALIZED;

	return 0;
}

int path_init2(struct path *p)
{
	int ret;

	assert(p->state == STATE_CHECKED);

	/* We sort the hooks according to their priority before starting the path */
	list_sort(&p->hooks, hook_cmp_priority);

	/* Initialize destinations */
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

		ret = path_destination_init(pd, p->queuelen);
		if (ret)
			return ret;
	}

	/* Initialize sources */
	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = list_at(&p->sources, i);

		ret = path_source_init(ps);
		if (ret)
			return ret;
	}

	/* Calc sample length of path */
	p->samplelen = 0;
	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = list_at(&p->sources, i);

		for (size_t i = 0; i < list_length(&ps->mappings); i++) {
			struct mapping_entry *me = list_at(&ps->mappings, i);

			if (me->offset + me->length > p->samplelen)
				p->samplelen = me->offset + me->length;
		}
	}

	if (!p->samplelen)
		p->samplelen = DEFAULT_SAMPLELEN;

	ret = pool_init(&p->pool, MAX(1, list_length(&p->destinations)) * p->queuelen, SAMPLE_LEN(p->samplelen), &memtype_hugepage);
	if (ret)
		return ret;

	sample_alloc(&p->pool, &p->last_sample, 1);

	/* Prepare poll() */
	p->reader.nfds = list_length(&p->sources);
	p->reader.pfds = alloc(sizeof(struct pollfd) * p->reader.nfds);

	for (int i = 0; i < p->reader.nfds; i++) {
		struct path_source *ps = list_at(&p->sources, i);

		p->reader.pfds[i].fd = node_fd(ps->node);
		p->reader.pfds[i].events = POLLIN;
	}

	return 0;
}

int path_parse(struct path *p, json_t *cfg, struct list *nodes)
{
	int ret;

	json_error_t err;
	json_t *json_in;
	json_t *json_out = NULL;
	json_t *json_hooks = NULL;

	struct list sources = { .state = STATE_DESTROYED };
	struct list destinations = { .state = STATE_DESTROYED };

	list_init(&sources);
	list_init(&destinations);

	ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s?: o, s?: o, s?: b, s?: b, s?: i, s?: i }",
		"in", &json_in,
		"out", &json_out,
		"hooks", &json_hooks,
		"reverse", &p->reverse,
		"enabled", &p->enabled,
		"queuelen", &p->queuelen
	);
	if (ret)
		jerror(&err, "Failed to parse path configuration");

	/* Input node(s) */
	ret = mapping_parse_list(&sources, json_in, nodes);
	if (ret)
		error("Failed to parse input mapping of path %s", path_name(p));

	/* Optional settings */

	/* Output node(s) */
	if (json_out) {
		ret = node_parse_list(&destinations, json_out, nodes);
		if (ret)
			jerror(&err, "Failed to parse output nodes");
	}

	for (size_t i = 0; i < list_length(&sources); i++) {
		struct mapping_entry *me = list_at(&sources, i);

		struct path_source *ps = NULL;

		/* Check if there is already a path_source for this source */
		for (size_t j = 0; j < list_length(&p->sources); j++) {
			struct path_source *pt = list_at(&p->sources, j);

			if (pt->node == me->node) {
				ps = pt;
				break;
			}
		}

		if (!ps) {
			ps = alloc(sizeof(struct path_source));

			ps->node = me->node;

			ps->mappings.state = STATE_DESTROYED;

			list_init(&ps->mappings);

			list_push(&p->sources, ps);
		}

		list_push(&ps->mappings, me);
	}

	for (size_t i = 0; i < list_length(&destinations); i++) {
		struct node *n = list_at(&destinations, i);

		struct path_destination *pd = alloc(sizeof(struct path_destination));

		pd->node = n;

		list_push(&p->destinations, pd);
	}

	if (json_hooks) {
		ret = hook_parse_list(&p->hooks, json_hooks, p, NULL);
		if (ret)
			return ret;
	}

	list_destroy(&sources, NULL, false);
	list_destroy(&destinations, NULL, false);

	p->cfg = cfg;
	p->state = STATE_PARSED;

	return 0;
}

int path_check(struct path *p)
{
	assert(p->state != STATE_DESTROYED);

	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = list_at(&p->sources, i);

		if (!ps->node->_vt->read)
			error("Source node '%s' is not supported as a source for path '%s'", node_name(ps->node), path_name(p));
	}

	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

		if (!pd->node->_vt->write)
			error("Destiation node '%s' is not supported as a sink for path '%s'", node_name(pd->node), path_name(p));
	}

	if (!IS_POW2(p->queuelen)) {
		p->queuelen = LOG2_CEIL(p->queuelen);
		warn("Queue length should always be a power of 2. Adjusting to %d", p->queuelen);
	}

	p->state = STATE_CHECKED;

	return 0;
}

int path_start(struct path *p)
{
	int ret;

	assert(p->state == STATE_CHECKED);

	info("Starting path %s: enabled=%s, reversed=%s, queuelen=%d, samplelen=%d, #hooks=%zu, #sources=%zu, #destinations=%zu",
		path_name(p),
		p->enabled ? "yes": "no",
		p->reverse ? "yes": "no",
		p->queuelen, p->samplelen,
		list_length(&p->hooks),
		list_length(&p->sources),
		list_length(&p->destinations)
	);

	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = list_at(&p->hooks, i);

		ret = hook_start(h);
		if (ret)
			return ret;
	}

	p->sequence = 0;

	/* Start one thread per path for sending to destinations */
	ret = pthread_create(&p->tid, NULL, &path_run, p);
	if (ret)
		return ret;

	p->state = STATE_STARTED;

	return 0;
}

int path_stop(struct path *p)
{
	int ret;

	if (p->state != STATE_STARTED)
		return 0;

	info("Stopping path: %s", path_name(p));

	ret = pthread_cancel(p->tid);
	if (ret)
		return ret;

	ret = pthread_join(p->tid, NULL);
	if (ret)
		return ret;

	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = list_at(&p->hooks, i);

		ret = hook_stop(h);
		if (ret)
			return ret;
	}

	p->state = STATE_STOPPED;

	return 0;
}

int path_destroy(struct path *p)
{
	if (p->state == STATE_DESTROYED)
		return 0;

	list_destroy(&p->hooks, (dtor_cb_t) hook_destroy, true);
	list_destroy(&p->sources, (dtor_cb_t) path_source_destroy, true);
	list_destroy(&p->destinations, (dtor_cb_t) path_destination_destroy, true);

	if (p->_name)
		free(p->_name);

	pool_destroy(&p->pool);

	p->state = STATE_DESTROYED;

	return 0;
}

const char * path_name(struct path *p)
{
	if (!p->_name) {
		strcatf(&p->_name, "[");

		for (size_t i = 0; i < list_length(&p->sources); i++) {
			struct path_source *ps = list_at(&p->sources, i);

			strcatf(&p->_name, " %s", node_name_short(ps->node));
		}

		strcatf(&p->_name, " ] " CLR_MAG("=>") " [");

		for (size_t i = 0; i < list_length(&p->destinations); i++) {
			struct path_destination *pd = list_at(&p->destinations, i);

			strcatf(&p->_name, " %s", node_name_short(pd->node));
		}

		strcatf(&p->_name, " ]");
	}

	return p->_name;
}

int path_uses_node(struct path *p, struct node *n)
{
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = list_at(&p->destinations, i);

		if (pd->node == n)
			return 0;
	}

	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = list_at(&p->sources, i);

		if (ps->node == n)
			return 0;
	}

	return -1;
}

int path_reverse(struct path *p, struct path *r)
{
	int ret;

	if (list_length(&p->destinations) != 1 || list_length(&p->sources) != 1)
		return -1;

	/* General */
	r->enabled = p->enabled;

	/* Source / Destinations */
	struct path_destination *orig_pd = list_first(&p->destinations);
	struct path_source      *orig_ps = list_first(&p->sources);

	struct path_destination *new_pd = alloc(sizeof(struct path_destination));
	struct path_source      *new_ps = alloc(sizeof(struct path_source));

	new_pd->node     = orig_ps->node;

	new_ps->node      = orig_pd->node;

	list_push(&r->destinations, new_pd);
	list_push(&r->sources, new_ps);

	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = list_at(&p->hooks, i);
		struct hook *g = alloc(sizeof(struct hook));

		ret = hook_init(g, h->_vt, r, NULL);
		if (ret)
			return ret;

		list_push(&r->hooks, g);
	}

	return 0;
}
