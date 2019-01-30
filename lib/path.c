/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/node/config.h>
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
#include <villas/signal.h>

/* Forward declaration */
static void path_destination_enqueue(struct path *p, struct sample *smps[], unsigned cnt);

static int path_source_init(struct path_source *ps)
{
	int ret;
	int pool_size = MAX(DEFAULT_QUEUE_LENGTH, ps->node->in.vectorize);

	if (ps->node->_vt->pool_size)
		pool_size = ps->node->_vt->pool_size;

	ret = pool_init(&ps->pool, pool_size, SAMPLE_LENGTH(vlist_length(&ps->node->signals)), node_memory_type(ps->node, &memory_hugepage));
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

	ret = vlist_destroy(&ps->mappings, NULL, true);
	if (ret)
		return ret;

	return 0;
}

static void path_source_read(struct path_source *ps, struct path *p, int i)
{
	int recv, tomux, allocated, cnt;
	unsigned release;

	cnt = ps->node->in.vectorize;

	struct sample *read_smps[cnt];
	struct sample *muxed_smps[cnt];
	struct sample **tomux_smps;

	/* Fill smps[] free sample blocks from the pool */
	allocated = sample_alloc_many(&ps->pool, read_smps, cnt);
	if (allocated != cnt)
		warning("Pool underrun for path source %s", node_name(ps->node));

	/* Read ready samples and store them to blocks pointed by smps[] */
	release = allocated;

	recv = node_read(ps->node, read_smps, allocated, &release);
	if (recv == 0)
		goto out2;
	else if (recv < 0)
		error("Failed to read samples from node %s", node_name(ps->node));
	else if (recv < allocated)
		warning("Partial read for path %s: read=%u, expected=%u", path_name(p), recv, allocated);

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

		muxed_smps[i]->flags = 0;
		if (p->original_sequence_no) {
			muxed_smps[i]->sequence = tomux_smps[i]->sequence;
			muxed_smps[i]->flags |= tomux_smps[i]->flags & SAMPLE_HAS_SEQUENCE;
		}
		else {
			muxed_smps[i]->sequence = p->last_sequence++;
			muxed_smps[i]->flags |= SAMPLE_HAS_SEQUENCE;
		}

		muxed_smps[i]->ts = tomux_smps[i]->ts;
		muxed_smps[i]->flags |= tomux_smps[i]->flags & (SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_TS_RECEIVED);

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

	sample_decref_many(muxed_smps, tomux);
out2:	sample_decref_many(read_smps, release);
}

static int path_destination_init(struct path_destination *pd, int queuelen)
{
	int ret;

	ret = queue_init(&pd->queue, queuelen, &memory_hugepage);
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
		warning("Pool underrun in path %s", path_name(p));

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

		enqueued = queue_push_many(&pd->queue, (void **) clones, cloned);
		if (enqueued != cnt)
			warning("Queue overrun for path %s", path_name(p));

		/* Increase reference counter of these samples as they are now also owned by the queue. */
		sample_incref_many(clones, cloned);

		debug(LOG_PATH | 15, "Enqueued %u samples to destination %s of path %s", enqueued, node_name(pd->node), path_name(p));
	}

	sample_decref_many(clones, cloned);
}

static void path_destination_write(struct path_destination *pd, struct path *p)
{
	int cnt = pd->node->out.vectorize;
	int sent;
	int released;
	int allocated;
	unsigned release;

	struct sample *smps[cnt];

	/* As long as there are still samples in the queue */
	while (1) {
		allocated = queue_pull_many(&pd->queue, (void **) smps, cnt);
		if (allocated == 0)
			break;
		else if (allocated < cnt)
			debug(LOG_PATH | 5, "Queue underrun for path %s: allocated=%u expected=%u", path_name(p), allocated, cnt);

		debug(LOG_PATH | 15, "Dequeued %u samples from queue of node %s which is part of path %s", allocated, node_name(pd->node), path_name(p));

		release = allocated;

		sent = node_write(pd->node, smps, allocated, &release);
		if (sent < 0)
			error("Failed to sent %u samples to node %s: reason=%d", cnt, node_name(pd->node), sent);
		else if (sent < allocated)
			warning("Partial write to node %s: written=%d, expected=%d", node_name(pd->node), sent, allocated);

		released = sample_decref_many(smps, release);

		debug(LOG_PATH | 15, "Released %d samples back to memory pool", released);
	}
}

static void * path_run_single(void *arg)
{
	struct path *p = arg;
	struct path_source *ps = (struct path_source *) vlist_at(&p->sources, 0);

	debug(1, "Started path %s in single mode", path_name(p));

	for (;;) {
		path_source_read(ps, p, 0);

		for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
			struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

			path_destination_write(pd, p);
		}

		pthread_testcancel();
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
			struct path_source *ps = (struct path_source *) vlist_at(&p->sources, i);

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

		for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
			struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

			path_destination_write(pd, p);
		}
	}

	return NULL;
}

int path_init(struct path *p)
{
	int ret;

	assert(p->state == STATE_DESTROYED);

	ret = vlist_init(&p->destinations);
	if (ret)
		return ret;

	ret = vlist_init(&p->sources);
	if (ret)
		return ret;

	ret = vlist_init(&p->signals);
	if (ret)
		return ret;

	ret = vlist_init(&p->hooks);
	if (ret)
		return ret;

	p->_name = NULL;

	/* Default values */
	p->mode = PATH_MODE_ANY;
	p->rate = 0; /* Disabled */

	p->builtin = 1;
	p->reverse = 0;
	p->enabled = 1;
	p->poll = -1;
	p->queuelen = DEFAULT_QUEUE_LENGTH;
	p->original_sequence_no = 0;

	p->state = STATE_INITIALIZED;

	return 0;
}

int path_init_poll(struct path *p)
{
	int ret, nfds;

	nfds = vlist_length(&p->sources);
	if (p->rate > 0)
		nfds++;

	p->reader.nfds = nfds;
	p->reader.pfds = alloc(sizeof(struct pollfd) * p->reader.nfds);

	for (int i = 0; i < vlist_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) vlist_at(&p->sources, i);

		int fds[16];
		int num_fds = node_poll_fds(ps->node, fds);

		for (int i = 0; i < num_fds; i++) {
			if (fds[i] < 0)
				error("Failed to get file descriptor for node %s", node_name(ps->node));

			/* This slot is only used if it is not masked */
			p->reader.pfds[i].events = POLLIN;
			p->reader.pfds[i].fd = fds[i];
		}
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
	/* Add internal hooks if they are not already in the list */
	ret = hook_init_builtin_list(&p->hooks, p->builtin, HOOK_PATH, p, NULL);
	if (ret)
		return ret;

	/* We sort the hooks according to their priority before starting the path */
	vlist_sort(&p->hooks, hook_cmp_priority);
#endif /* WITH_HOOKS */

	/* Initialize destinations */
	struct memory_type *pool_mt = &memory_hugepage;
	int pool_size = MAX(1, vlist_length(&p->destinations)) * p->queuelen;

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

		if (pd->node->_vt->pool_size > pool_size)
			pool_size = pd->node->_vt->pool_size;

		if (pd->node->_vt->memory_type)
			pool_mt = node_memory_type(pd->node, &memory_hugepage);

		ret = path_destination_init(pd, p->queuelen);
		if (ret)
			return ret;
	}

	bitset_init(&p->received, vlist_length(&p->sources));
	bitset_init(&p->mask, vlist_length(&p->sources));

	/* Initialize sources */
	for (size_t i = 0; i < vlist_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) vlist_at(&p->sources, i);

		ret = path_source_init(ps);
		if (ret)
			return ret;

		if (ps->masked)
			bitset_set(&p->mask, i);

		for (size_t i = 0; i < vlist_length(&ps->mappings); i++) {
			struct mapping_entry *me = (struct mapping_entry *) vlist_at(&ps->mappings, i);

			int off = me->offset;
			int len = me->length;

			for (int j = 0; j < len; j++) {
				struct signal *sig;

				/* For data mappings we simple refer to the existing
				 * signal descriptors of the source node. */
				if (me->type == MAPPING_TYPE_DATA) {
					sig = (struct signal *) vlist_at_safe(&me->node->signals, me->data.offset + j);
					if (!sig) {
						warning("Failed to create signal description for path %s", path_name(p));
						continue;
					}

					signal_incref(sig);
				}
				/* For other mappings we create new signal descriptors */
				else {
					sig = alloc(sizeof(struct signal));

					ret = signal_init_from_mapping(sig, me, j);
					if (ret)
						return -1;
				}

				vlist_extend(&p->signals, off + j + 1, NULL);
				vlist_set(&p->signals, off + j, sig);
			}
		}
	}

	ret = pool_init(&p->pool, pool_size, SAMPLE_LENGTH(vlist_length(&p->signals)), pool_mt);
	if (ret)
		return ret;

	/* Prepare poll() */
	if (p->poll) {
		ret = path_init_poll(p);
		if (ret)
			return ret;
	}

	return 0;
}

int path_parse(struct path *p, json_t *cfg, struct vlist *nodes)
{
	int ret;

	json_error_t err;
	json_t *json_in;
	json_t *json_out = NULL;
	json_t *json_hooks = NULL;
	json_t *json_mask = NULL;

	const char *mode = NULL;

	struct vlist sources = { .state = STATE_DESTROYED };
	struct vlist destinations = { .state = STATE_DESTROYED };

	vlist_init(&sources);
	vlist_init(&destinations);

	ret = json_unpack_ex(cfg, &err, 0, "{ s: o, s?: o, s?: o, s?: b, s?: b, s?: b, s?: i, s?: s, s?: b, s?: F, s?: o, s?: b}",
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
		"mask", &json_mask,
		"original_sequence_no", &p->original_sequence_no

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

	for (size_t i = 0; i < vlist_length(&sources); i++) {
		struct mapping_entry *me = (struct mapping_entry *) vlist_at(&sources, i);
		struct path_source *ps = NULL;

		/* Check if there is already a path_source for this source */
		for (size_t j = 0; j < vlist_length(&p->sources); j++) {
			struct path_source *pt = (struct path_source *) vlist_at(&p->sources, j);

			if (pt->node == me->node) {
				ps = pt;
				break;
			}
		}

		/* Create new path_source of not existing */
		if (!ps) {
			ps = alloc(sizeof(struct path_source));

			ps->node = me->node;
			ps->masked = false;

			ps->mappings.state = STATE_DESTROYED;

			vlist_init(&ps->mappings);

			vlist_push(&p->sources, ps);
		}

		vlist_push(&ps->mappings, me);
	}

	for (size_t i = 0; i < vlist_length(&destinations); i++) {
		struct node *n = (struct node *) vlist_at(&destinations, i);

		struct path_destination *pd = (struct path_destination *) alloc(sizeof(struct path_destination));

		pd->node = n;

		vlist_push(&p->destinations, pd);
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

			node = vlist_lookup(nodes, name);
			if (!node)
				error("The 'mask' entry '%s' is not a valid node name", name);

			/* Search correspondending path_source to node */
			for (size_t i = 0; i < vlist_length(&p->sources); i++) {
				struct path_source *pt = (struct path_source *) vlist_at(&p->sources, i);

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
	/* Enable all by default */
	else {
		for (size_t i = 0; i < vlist_length(&p->sources); i++) {
			struct path_source *ps = (struct path_source *) vlist_at(&p->sources, i);

			ps->masked = true;
		}
	}

#ifdef WITH_HOOKS
	if (json_hooks) {
		ret = hook_parse_list(&p->hooks, json_hooks, HOOK_PATH, p, NULL);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	/* Autodetect whether to use poll() for this path or not */
	if (p->poll == -1) {
		struct path_source *ps = (struct path_source *) vlist_at(&p->sources, 0);

		int fds[16];
		int num_fds = node_poll_fds(ps->node, fds);

		p->poll = (p->rate > 0 || vlist_length(&p->sources) > 1) && num_fds > 0;
	}

	ret = vlist_destroy(&sources, NULL, false);
	if (ret)
		return ret;

	ret = vlist_destroy(&destinations, NULL, false);
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

	if (p->poll) {
		if (p->rate <= 0) {
			/* Check that all path sources provide a file descriptor for polling */
			for (size_t i = 0; i < vlist_length(&p->sources); i++) {
				struct path_source *ps = (struct path_source *) vlist_at(&p->sources, i);

				if (!ps->node->_vt->poll_fds)
					error("Node %s can not be used in polling mode with path %s", node_name(ps->node), path_name(p));
			}
		}
	}
	else {
		/* Check that we do not need to multiplex between multiple sources when polling is disabled */
		if (vlist_length(&p->sources) > 1)
			error("Setting 'poll' must be active if the path has more than one source");

		/* Check that we do not use the fixed rate feature when polling is disabled */
		if (p->rate > 0)
			error("Setting 'poll' must be activated when used together with setting 'rate'");
	}

	for (size_t i = 0; i < vlist_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) vlist_at(&p->sources, i);

		if (!ps->node->_vt->read)
			error("Node %s is not supported as a source for path %s", node_name(ps->node), path_name(p));
	}

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

		if (!pd->node->_vt->write)
			error("Destiation node %s is not supported as a sink for path %s", node_name(pd->node), path_name(p));
	}

	if (!IS_POW2(p->queuelen)) {
		p->queuelen = LOG2_CEIL(p->queuelen);
		warning("Queue length should always be a power of 2. Adjusting to %d", p->queuelen);
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

	info("Starting path %s: #signals=%zu, mode=%s, poll=%s, mask=%s, rate=%.2f, enabled=%s, reversed=%s, queuelen=%d, #hooks=%zu, #sources=%zu, #destinations=%zu, original_sequence_no=%s",
		path_name(p),
		vlist_length(&p->signals),
		mode,
		p->poll ? "yes" : "no",
		mask,
		p->rate,
		p->enabled ? "yes" : "no",
		p->reverse ? "yes" : "no",
		p->queuelen,
		vlist_length(&p->hooks),
		vlist_length(&p->sources),
		vlist_length(&p->destinations),
		p->original_sequence_no ? "yes" : "no"
	);

	free(mask);

#ifdef WITH_HOOKS
	for (size_t i = 0; i < vlist_length(&p->hooks); i++) {
		struct hook *h = (struct hook *) vlist_at(&p->hooks, i);

		ret = hook_start(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	p->last_sequence = 0;

	bitset_clear_all(&p->received);

	/* We initialize the intial sample */
	p->last_sample = sample_alloc(&p->pool);
	if (!p->last_sample)
		return -1;

	p->last_sample->length = vlist_length(&p->signals);
	p->last_sample->signals = &p->signals;
	p->last_sample->sequence = 0;
	p->last_sample->flags = p->last_sample->length > 0 ? SAMPLE_HAS_DATA : 0;

	for (size_t i = 0; i < p->last_sample->length; i++) {
		struct signal *sig = (struct signal *) vlist_at(p->last_sample->signals, i);

		p->last_sample->data[i] = sig->init;
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
	for (size_t i = 0; i < vlist_length(&p->hooks); i++) {
		struct hook *h = (struct hook *) vlist_at(&p->hooks, i);

		ret = hook_stop(h);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	sample_decref(p->last_sample);

	p->state = STATE_STOPPED;

	return 0;
}

int path_destroy(struct path *p)
{
	if (p->state == STATE_DESTROYED)
		return 0;

#ifdef WITH_HOOKS
	vlist_destroy(&p->hooks, (dtor_cb_t) hook_destroy, true);
#endif
	vlist_destroy(&p->sources, (dtor_cb_t) path_source_destroy, true);
	vlist_destroy(&p->destinations, (dtor_cb_t) path_destination_destroy, true);
	vlist_destroy(&p->signals, (dtor_cb_t) signal_decref, false);

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

		for (size_t i = 0; i < vlist_length(&p->sources); i++) {
			struct path_source *ps = (struct path_source *) vlist_at(&p->sources, i);

			strcatf(&p->_name, " %s", node_name_short(ps->node));
		}

		strcatf(&p->_name, " ] " CLR_MAG("=>") " [");

		for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
			struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

			strcatf(&p->_name, " %s", node_name_short(pd->node));
		}

		strcatf(&p->_name, " ]");
	}

	return p->_name;
}

int path_uses_node(struct path *p, struct node *n)
{
	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

		if (pd->node == n)
			return 0;
	}

	for (size_t i = 0; i < vlist_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) vlist_at(&p->sources, i);

		if (ps->node == n)
			return 0;
	}

	return -1;
}

int path_reverse(struct path *p, struct path *r)
{
	if (vlist_length(&p->destinations) != 1 || vlist_length(&p->sources) != 1)
		return -1;

	/* General */
	r->enabled = p->enabled;

	/* Source / Destinations */
	struct path_destination *orig_pd = vlist_first(&p->destinations);
	struct path_source      *orig_ps = vlist_first(&p->sources);

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

	vlist_init(&new_ps->mappings);
	vlist_push(&new_ps->mappings, new_me);

	vlist_push(&r->destinations, new_pd);
	vlist_push(&r->sources, new_ps);

#ifdef WITH_HOOKS
	for (size_t i = 0; i < vlist_length(&p->hooks); i++) {
		int ret;

		struct hook *h = (struct hook *) vlist_at(&p->hooks, i);
		struct hook *g = (struct hook *) alloc(sizeof(struct hook));

		ret = hook_init(g, h->_vt, r, NULL);
		if (ret)
			return ret;

		vlist_push(&r->hooks, g);
	}
#endif /* WITH_HOOKS */
	return 0;
}
