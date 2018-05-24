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

#include <villas/config.h>
#include <villas/utils.h>
#include <villas/path.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/queue.h>
#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/memory.h>
#include <villas/stats.h>
#include <villas/node.h>

/* Forward declaration */
static void path_destination_enqueue(struct path *p, struct sample *smps[], unsigned cnt);

static int path_source_init(struct path_source *ps)
{
	int ret;

	ret = pool_init(&ps->pool, MAX(DEFAULT_QUEUELEN, ps->node->in.vectorize), SAMPLE_LEN(ps->node->samplelen), &memtype_hugepage);
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

	ret = list_destroy(&ps->mappings, NULL, true);
	if (ret)
		return ret;

	return 0;
}

static void path_source_read(struct path_source *ps, struct path *p, int i)
{
	int recv, tomux, ready, cnt;

	cnt = ps->node->in.vectorize;

	struct sample *read_smps[cnt];
	struct sample *muxed_smps[cnt];
	struct sample **tomux_smps;

	/* Fill smps[] free sample blocks from the pool */
	ready = sample_alloc_many(&ps->pool, read_smps, cnt);
	if (ready != cnt)
		warn("Pool underrun for path source %s", node_name(ps->node));

	/* Read ready samples and store them to blocks pointed by smps[] */
	recv = node_read(ps->node, read_smps, ready);
	if (recv == 0)
		goto out2;
	else if (recv < 0)
		error("Failed to read samples from node %s", node_name(ps->node));
	else if (recv < ready)
		warn("Partial read for path %s: read=%u, expected=%u", path_name(p), recv, ready);

	bitset_set(&p->received, i);

	if (p->mode == PATH_MODE_ANY) { /* Mux all samples */
		tomux_smps = read_smps;
		tomux = recv;
	}
	else { /* Mux only last sample and discard others */
		tomux_smps = read_smps + recv - 1;
		tomux = 1;
	}

	for (int i = 0; i < tomux; i++) {
		muxed_smps[i] = i == 0
			? sample_clone(p->last_sample)
			: sample_clone(muxed_smps[i-1]);

		muxed_smps[i]->sequence = p->last_sequence + 1;

		mapping_remap(&ps->mappings, muxed_smps[i], tomux_smps[i], NULL);
	}

	sample_copy(p->last_sample, muxed_smps[tomux-1]);

	debug(15, "Path %s received = %s", path_name(p), bitset_dump(&p->received));

#ifdef WITH_HOOKS
	int toenqueue = hook_process_list(&p->hooks, muxed_smps, tomux);
        if (toenqueue != tomux) {
                int skipped = tomux - toenqueue;

                debug(LOG_NODES | 10, "Hooks skipped %u out of %u samples for path %s", skipped, tomux, path_name(p));
        }
#else
	int toenqueue = tomux;
#endif

	if (bitset_test(&p->mask, i)) {
		/* Check if we received an update from all nodes/ */
		if ((p->mode == PATH_MODE_ANY) ||
		    (p->mode == PATH_MODE_ALL && !bitset_cmp(&p->mask, &p->received)))
		{
			path_destination_enqueue(p, muxed_smps, toenqueue);

			/* Reset bitset of updated nodes */
			bitset_clear_all(&p->received);
		}
	}

	sample_put_many(muxed_smps, tomux);
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

static void path_destination_enqueue(struct path *p, struct sample *smps[], unsigned cnt)
{
	unsigned enqueued, cloned;

	struct sample *clones[cnt];

	cloned = sample_clone_many(clones, smps, cnt);
	if (cloned < cnt)
		warn("Pool underrun in path %s", path_name(p));

	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

		enqueued = queue_push_many(&pd->queue, (void **) clones, cloned);
		if (enqueued != cnt)
			warn("Queue overrun for path %s", path_name(p));

		/* Increase reference counter of these samples as they are now also owned by the queue. */
		sample_get_many(clones, cloned);

		debug(LOG_PATH | 15, "Enqueued %u samples to destination %s of path %s", enqueued, node_name(pd->node), path_name(p));
	}

	sample_put_many(clones, cloned);
}

static void path_destination_write(struct path_destination *pd, struct path *p)
{
	int cnt = pd->node->out.vectorize;
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

static void * path_run_single(void *arg)
{
	struct path *p = arg;
	struct path_source *ps = (struct path_source *) list_at(&p->sources, 0);

	debug(1, "Started path %s in single mode", path_name(p));

	for (;;) {
		path_source_read(ps, p, 0);

		for (size_t i = 0; i < list_length(&p->destinations); i++) {
			struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

			path_destination_write(pd, p);
		}
	}

	return NULL;
}

/** Main thread function per path: read samples -> write samples */
static void * path_run_poll(void *arg)
{
	int ret;
	struct path *p = arg;

	debug(1, "Started path %s in polling mode", path_name(p));

	for (;;) {
		ret = poll(p->reader.pfds, p->reader.nfds, -1);
		if (ret < 0)
			serror("Failed to poll");

		debug(10, "Path %s returned from poll(2)", path_name(p));

		for (int i = 0; i < p->reader.nfds; i++) {
			struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

			if (p->reader.pfds[i].revents & POLLIN) {
				/* Timeout: re-enqueue the last sample */
				if (p->reader.pfds[i].fd == task_fd(&p->timeout)) {
					task_wait(&p->timeout);

					p->last_sample->sequence = p->last_sequence++;

					path_destination_enqueue(p, &p->last_sample, 1);
				}
				/* A source is ready to receive samples */
				else {
					path_source_read(ps, p, i);
				}
			}
		}

		for (size_t i = 0; i < list_length(&p->destinations); i++) {
			struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

			path_destination_write(pd, p);
		}
	}

	return NULL;
}

int path_init(struct path *p)
{
	assert(p->state == STATE_DESTROYED);

	list_init(&p->destinations);
	list_init(&p->sources);

	p->_name = NULL;

	/* Default values */
	p->mode = PATH_MODE_ANY;
	p->rate = 0; /* Disabled */

	p->builtin = 1;
	p->reverse = 0;
	p->enabled = 1;
	p->poll = -1;
	p->queuelen = DEFAULT_QUEUELEN;

#ifdef WITH_HOOKS
	/* Add internal hooks if they are not already in the list */
	list_init(&p->hooks);
	if (p->builtin) {
		int ret;

		for (size_t i = 0; i < list_length(&plugins); i++) {
			struct plugin *q = (struct plugin *) list_at(&plugins, i);

			if (q->type != PLUGIN_TYPE_HOOK)
				continue;

			struct hook_type *vt = &q->hook;

			if (!(vt->flags & HOOK_PATH) || !(vt->flags & HOOK_BUILTIN))
				continue;

			struct hook *h = (struct hook *) alloc(sizeof(struct hook));

			ret = hook_init(h, vt, p, NULL);
			if (ret)
				return ret;

			list_push(&p->hooks, h);
		}
	}
#endif /* WITH_HOOKS */

	p->state = STATE_INITIALIZED;

	return 0;
}

int path_init_poll(struct path *p)
{
	int ret, nfds;

	nfds = list_length(&p->sources);
	if (p->rate > 0)
		nfds++;

	p->reader.nfds = nfds;
	p->reader.pfds = alloc(sizeof(struct pollfd) * p->reader.nfds);

	for (int i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

		/* This slot is only used if it is not masked */
		p->reader.pfds[i].events = POLLIN;
		p->reader.pfds[i].fd = node_fd(ps->node);

		if (p->reader.pfds[i].fd < 0)
			error("Failed to get file descriptor for node %s", node_name(ps->node));
	}

	/* We use the last slot for the timeout timer. */
	if (p->rate > 0) {
		ret = task_init(&p->timeout, p->rate, CLOCK_MONOTONIC);
		if (ret)
			return ret;

		p->reader.pfds[nfds-1].events = POLLIN;
		p->reader.pfds[nfds-1].fd = task_fd(&p->timeout);
		if (p->reader.pfds[nfds-1].fd < 0)
			error("Failed to get file descriptor for timer of path %s", path_name(p));
	}

	return 0;
}

int path_init2(struct path *p)
{
	int ret;

	assert(p->state == STATE_CHECKED);

#ifdef WITH_HOOKS
	/* We sort the hooks according to their priority before starting the path */
	list_sort(&p->hooks, hook_cmp_priority);
#endif /* WITH_HOOKS */

	/* Initialize destinations */
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

		ret = path_destination_init(pd, p->queuelen);
		if (ret)
			return ret;
	}

	/* Initialize sources */
	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

		ret = path_source_init(ps);
		if (ret)
			return ret;
	}

	bitset_init(&p->received, list_length(&p->sources));
	bitset_init(&p->mask, list_length(&p->sources));

	/* Calc sample length of path and initialize bitset */
	p->samplelen = 0;
	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

		if (ps->masked)
			bitset_set(&p->mask, i);

		for (size_t i = 0; i < list_length(&ps->mappings); i++) {
			struct mapping_entry *me = (struct mapping_entry *) list_at(&ps->mappings, i);

			int len = me->length;
			int off = me->offset;

			if (off + len > p->samplelen)
				p->samplelen = off + len;
		}
	}

	if (!p->samplelen)
		p->samplelen = DEFAULT_SAMPLELEN;

	ret = pool_init(&p->pool, MAX(1, list_length(&p->destinations)) * p->queuelen, SAMPLE_LEN(p->samplelen), &memtype_hugepage);
	if (ret)
		return ret;

	p->last_sample = sample_alloc(&p->pool);
	if (!p->last_sample)
		return -1;

	/* Prepare poll() */
	if (p->poll) {
		ret = path_init_poll(p);
		if (ret)
			return ret;
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
	json_t *json_mask = NULL;

	const char *mode = NULL;

	struct list sources = { .state = STATE_DESTROYED };
	struct list destinations = { .state = STATE_DESTROYED };

	list_init(&sources);
	list_init(&destinations);

	ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s?: o, s?: o, s?: b, s?: b, s?: b, s?: i, s?: s, s?: b, s?: F, s?: o }",
		"in", &json_in,
		"out", &json_out,
		"hooks", &json_hooks,
		"reverse", &p->reverse,
		"enabled", &p->enabled,
		"builtin", &p->builtin,
		"queuelen", &p->queuelen,
		"mode", &mode,
		"poll", &p->poll,
		"rate", &p->rate,
		"mask", &json_mask
	);
	if (ret)
		jerror(&err, "Failed to parse path configuration");

	/* Input node(s) */
	ret = mapping_parse_list(&sources, json_in, nodes);
	if (ret)
		error("Failed to parse input mapping of path %s", path_name(p));

	/* Optional settings */
	if (mode) {
		if      (!strcmp(mode, "any"))
			p->mode = PATH_MODE_ANY;
		else if (!strcmp(mode, "all"))
			p->mode = PATH_MODE_ALL;
		else
			error("Invalid path mode '%s'", mode);
	}

	/* Output node(s) */
	if (json_out) {
		ret = node_parse_list(&destinations, json_out, nodes);
		if (ret)
			jerror(&err, "Failed to parse output nodes");
	}

	for (size_t i = 0; i < list_length(&sources); i++) {
		struct mapping_entry *me = (struct mapping_entry *) list_at(&sources, i);

		struct path_source *ps = NULL;

		/* Check if there is already a path_source for this source */
		for (size_t j = 0; j < list_length(&p->sources); j++) {
			struct path_source *pt = (struct path_source *) list_at(&p->sources, j);

			if (pt->node == me->node) {
				ps = pt;
				break;
			}
		}

		if (!ps) {
			ps = alloc(sizeof(struct path_source));

			ps->node = me->node;
			ps->masked = false;

			ps->mappings.state = STATE_DESTROYED;

			list_init(&ps->mappings);

			list_push(&p->sources, ps);
		}

		list_push(&ps->mappings, me);
	}

	for (size_t i = 0; i < list_length(&destinations); i++) {
		struct node *n = (struct node *) list_at(&destinations, i);

		struct path_destination *pd = (struct path_destination *) alloc(sizeof(struct path_destination));

		pd->node = n;

		list_push(&p->destinations, pd);
	}

	if (json_mask) {
		json_t *json_entry;
		size_t i;

		if (!json_is_array(json_mask))
			error("The 'mask' setting must be a list of node names");

		json_array_foreach(json_mask, i, json_entry) {
			const char *name;
			struct node *node;
			struct path_source *ps = NULL;

			name = json_string_value(json_entry);
			if (!name)
				error("The 'mask' setting must be a list of node names");

			node = list_lookup(nodes, name);
			if (!node)
				error("The 'mask' entry '%s' is not a valid node name", name);

			/* Search correspondending path_source to node */
			for (size_t i = 0; i < list_length(&p->sources); i++) {
				struct path_source *pt = (struct path_source *) list_at(&p->sources, i);

				if (pt->node == node) {
					ps = pt;
					break;
				}
			}

			if (!ps)
				error("Node %s is not a source of the path %s", node_name(node), path_name(p));

			ps->masked = true;
		}
	}
	else {/* Enable all by default */
		for (size_t i = 0; i < list_length(&p->sources); i++) {
			struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

			ps->masked = true;
		}
	}

#if WITH_HOOKS
	if (json_hooks) {
		ret = hook_parse_list(&p->hooks, json_hooks, p, NULL);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	/* Autodetect whether to use poll() for this path or not */
	if (p->poll == -1) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, 0);

		p->poll = (
			     p->rate > 0 ||
		             list_length(&p->sources) > 1
		          )
			  && node_fd(ps->node) != 1;
	}

	ret = list_destroy(&sources, NULL, false);
	if (ret)
		return ret;

	ret = list_destroy(&destinations, NULL, false);
	if (ret)
		return ret;

	p->cfg = cfg;
	p->state = STATE_PARSED;

	return 0;
}

int path_check(struct path *p)
{
	assert(p->state != STATE_DESTROYED);

	if (p->rate < 0)
		error("Setting 'rate' of path %s must be a positive number.", path_name(p));

	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

		if (!ps->node->_vt->read)
			error("Source node '%s' is not supported as a source for path '%s'", node_name(ps->node), path_name(p));
	}

	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

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
	char *mode, *mask;

	assert(p->state == STATE_CHECKED);

	switch (p->mode) {
		case PATH_MODE_ANY: mode = "any";     break;
		case PATH_MODE_ALL: mode = "all";     break;
		default:            mode = "unknown"; break;
	}

	mask = bitset_dump(&p->mask);

	info("Starting path %s: mode=%s, poll=%s, mask=%s, rate=%.2f, enabled=%s, reversed=%s, queuelen=%d, samplelen=%d, #hooks=%zu, #sources=%zu, #destinations=%zu",
		path_name(p),
		mode,
		p->poll ? "yes" : "no",
		mask,
		p->rate,
		p->enabled ? "yes" : "no",
		p->reverse ? "yes" : "no",
		p->queuelen, p->samplelen,
		list_length(&p->hooks),
		list_length(&p->sources),
		list_length(&p->destinations)
	);

	free(mask);

#ifdef WITH_HOOKS
	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = (struct hook *) list_at(&p->hooks, i);

		ret = hook_start(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	p->last_sequence = 0;

	bitset_clear_all(&p->received);

	/* We initialize the intial sample with zeros */
	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

		for (size_t j = 0; j < list_length(&ps->mappings); j++) {
			struct mapping_entry *me = (struct mapping_entry *) list_at(&ps->mappings, j);

			int len = me->length;
			int off = me->offset;

			if (len + off > p->last_sample->length)
				p->last_sample->length = len + off;

			for (int k = off; k < off + len; k++) {
				p->last_sample->data[k].f = 0;

				sample_set_data_format(p->last_sample, k, SAMPLE_DATA_FORMAT_FLOAT);
			}
		}
	}

	/* Start one thread per path for sending to destinations
	 *
	 * Special case: If the path only has a single source and this source
	 * does not offer a file descriptor for polling, we will use a special
	 * thread function.
	 */
	ret = pthread_create(&p->tid, NULL, p->poll ? path_run_poll : path_run_single, p);
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

#ifdef WITH_HOOKS
	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		struct hook *h = (struct hook *) list_at(&p->hooks, i);

		ret = hook_stop(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	p->state = STATE_STOPPED;

	return 0;
}

int path_destroy(struct path *p)
{
	if (p->state == STATE_DESTROYED)
		return 0;

#ifdef WITH_HOOKS
	list_destroy(&p->hooks, (dtor_cb_t) hook_destroy, true);
#endif
	list_destroy(&p->sources, (dtor_cb_t) path_source_destroy, true);
	list_destroy(&p->destinations, (dtor_cb_t) path_destination_destroy, true);

	if (p->reader.pfds)
		free(p->reader.pfds);

	if (p->_name)
		free(p->_name);

	if (p->rate > 0)
		task_destroy(&p->timeout);

	pool_destroy(&p->pool);

	p->state = STATE_DESTROYED;

	return 0;
}

const char * path_name(struct path *p)
{
	if (!p->_name) {
		strcatf(&p->_name, "[");

		for (size_t i = 0; i < list_length(&p->sources); i++) {
			struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

			strcatf(&p->_name, " %s", node_name_short(ps->node));
		}

		strcatf(&p->_name, " ] " CLR_MAG("=>") " [");

		for (size_t i = 0; i < list_length(&p->destinations); i++) {
			struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

			strcatf(&p->_name, " %s", node_name_short(pd->node));
		}

		strcatf(&p->_name, " ]");
	}

	return p->_name;
}

int path_uses_node(struct path *p, struct node *n)
{
	for (size_t i = 0; i < list_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) list_at(&p->destinations, i);

		if (pd->node == n)
			return 0;
	}

	for (size_t i = 0; i < list_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) list_at(&p->sources, i);

		if (ps->node == n)
			return 0;
	}

	return -1;
}

int path_reverse(struct path *p, struct path *r)
{
	if (list_length(&p->destinations) != 1 || list_length(&p->sources) != 1)
		return -1;

	/* General */
	r->enabled = p->enabled;

	/* Source / Destinations */
	struct path_destination *orig_pd = list_first(&p->destinations);
	struct path_source      *orig_ps = list_first(&p->sources);

	struct path_destination *new_pd = (struct path_destination *) alloc(sizeof(struct path_destination));
	struct path_source      *new_ps = (struct path_source *) alloc(sizeof(struct path_source));
	struct mapping_entry    *new_me = alloc(sizeof(struct mapping_entry));
	new_pd->node = orig_ps->node;
	new_ps->node = orig_pd->node;
	new_ps->masked = true;

	new_me->node = new_ps->node;
	new_me->type = MAPPING_TYPE_DATA;
	new_me->data.offset = 0;
	new_me->length = 0;

	list_init(&new_ps->mappings);
	list_push(&new_ps->mappings, new_me);

	list_push(&r->destinations, new_pd);
	list_push(&r->sources, new_ps);

#ifdef WITH_HOOKS
	for (size_t i = 0; i < list_length(&p->hooks); i++) {
		int ret;

		struct hook *h = (struct hook *) list_at(&p->hooks, i);
		struct hook *g = (struct hook *) alloc(sizeof(struct hook));

		ret = hook_init(g, h->_vt, r, NULL);
		if (ret)
			return ret;

		list_push(&r->hooks, g);
	}
#endif /* WITH_HOOKS */
	return 0;
}
