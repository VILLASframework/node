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
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/queue.h>
#include <villas/hook_list.h>
#include <villas/plugin.h>
#include <villas/memory.h>
#include <villas/stats.h>
#include <villas/node.h>
#include <villas/signal.h>
#include <villas/path.h>
#include <villas/path_source.h>
#include <villas/path_destination.h>

static void * path_run_single(void *arg)
{
	int ret;
	struct path *p = arg;
	struct path_source *ps = (struct path_source *) vlist_at(&p->sources, 0);

	while (p->state == STATE_STARTED) {
		pthread_testcancel();

		ret = path_source_read(ps, p, 0);
		if (ret <= 0)
			continue;

		for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
			struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

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

	while (p->state == STATE_STARTED) {
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
				else
					path_source_read(ps, p, i);
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

	ret = signal_list_init(&p->signals);
	if (ret)
		return ret;

	ret = hook_list_init(&p->hooks);
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

static int path_prepare_poll(struct path *p)
{
	int fds[16], ret, n = 0, m;

	p->reader.pfds = NULL;
	p->reader.nfds = 0;

	for (int i = 0; i < vlist_length(&p->sources); i++) {
		struct path_source *ps = (struct path_source *) vlist_at(&p->sources, i);

		m = node_poll_fds(ps->node, fds);
		if (m < 0)
			continue;

		p->reader.nfds += m;
		p->reader.pfds = realloc(p->reader.pfds, p->reader.nfds * sizeof(struct pollfd));

		for (int i = 0; i < m; i++) {
			if (fds[i] < 0)
				error("Failed to get file descriptor for node %s", node_name(ps->node));

			/* This slot is only used if it is not masked */
			p->reader.pfds[n].events = POLLIN;
			p->reader.pfds[n++].fd = fds[i];
		}
	}

	/* We use the last slot for the timeout timer. */
	if (p->rate > 0) {
		ret = task_init(&p->timeout, p->rate, CLOCK_MONOTONIC);
		if (ret)
			return ret;

		p->reader.nfds++;
		p->reader.pfds = realloc(p->reader.pfds, p->reader.nfds * sizeof(struct pollfd));

		p->reader.pfds[p->reader.nfds-1].events = POLLIN;
		p->reader.pfds[p->reader.nfds-1].fd = task_fd(&p->timeout);
		if (p->reader.pfds[p->reader.nfds-1].fd < 0)
			error("Failed to get file descriptor for timer of path %s", path_name(p));
	}

	return 0;
}

int path_prepare(struct path *p)
{
	int ret;

	assert(p->state == STATE_CHECKED);

#ifdef WITH_HOOKS
	int m = p->builtin ? HOOK_PATH | HOOK_BUILTIN : 0;

	/* Add internal hooks if they are not already in the list */
	ret = hook_list_prepare(&p->hooks, &p->signals, m, p, NULL);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	/* Initialize destinations */
	struct memory_type *pool_mt = &memory_hugepage;
	int pool_size = MAX(1, vlist_length(&p->destinations)) * p->queuelen;

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

		if (node_type(pd->node)->pool_size > pool_size)
			pool_size = node_type(pd->node)->pool_size;

		if (node_type(pd->node)->memory_type)
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

		ret = mapping_list_prepare(&ps->mappings);
		if (ret)
			return ret;

		for (size_t i = 0; i < vlist_length(&ps->mappings); i++) {
			struct mapping_entry *me = (struct mapping_entry *) vlist_at(&ps->mappings, i);
			struct vlist *sigs = node_get_signals(me->node, NODE_DIR_IN);

			for (int j = 0; j < me->length; j++) {
				struct signal *sig;

				/* For data mappings we simple refer to the existing
				 * signal descriptors of the source node. */
				if (me->type == MAPPING_TYPE_DATA) {
					sig = (struct signal *) vlist_at_safe(sigs, me->data.offset + j);
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

				vlist_extend(&p->signals, me->offset + j + 1, NULL);
				vlist_set(&p->signals, me->offset + j, sig);
			}
		}
	}

	ret = pool_init(&p->pool, pool_size, SAMPLE_LENGTH(vlist_length(&p->signals)), pool_mt);
	if (ret)
		return ret;

	/* Prepare poll() */
	if (p->poll) {
		ret = path_prepare_poll(p);
		if (ret)
			return ret;
	}

	p->state = STATE_PREPARED;

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
	ret = mapping_list_parse(&sources, json_in, nodes);
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
		ret = node_list_parse(&destinations, json_out, nodes);
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

		if (!node_is_enabled(ps->node))
			error("Source %s of path %s is not enabled", node_name(ps->node), path_name(p));

		vlist_push(&ps->mappings, me);
	}

	for (size_t i = 0; i < vlist_length(&destinations); i++) {
		struct node *n = (struct node *) vlist_at(&destinations, i);

		struct path_destination *pd = (struct path_destination *) alloc(sizeof(struct path_destination));

		pd->node = n;

		if (!node_is_enabled(pd->node))
			error("Destination %s of path %s is not enabled", node_name(pd->node), path_name(p));

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
		ret = hook_list_parse(&p->hooks, json_hooks, HOOK_PATH, p, NULL);
		if (ret)
			return ret;
	}
#endif /* WITH_HOOKS */

	/* Autodetect whether to use poll() for this path or not */
	if (p->poll == -1) {
		if (p->rate > 0)
			p->poll = 1;
		else if (vlist_length(&p->sources) > 1)
			p->poll = 1;
		else
			p->poll = 0;
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

				if (!node_type(ps->node)->poll_fds)
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

		if (!node_type(ps->node)->read)
			error("Node %s is not supported as a source for path %s", node_name(ps->node), path_name(p));
	}

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct path_destination *pd = (struct path_destination *) vlist_at(&p->destinations, i);

		if (!node_type(pd->node)->write)
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

	assert(p->state == STATE_PREPARED);

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
		path_is_enabled(p) ? "yes" : "no",
		path_is_reversed(p) ? "yes" : "no",
		p->queuelen,
		vlist_length(&p->hooks),
		vlist_length(&p->sources),
		vlist_length(&p->destinations),
		p->original_sequence_no ? "yes" : "no"
	);

	free(mask);

#ifdef WITH_HOOKS
	ret = hook_list_start(&p->hooks);
	if (ret)
		return ret;
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

	if (p->state != STATE_STARTED && p->state != STATE_STOPPING)
		return 0;

	info("Stopping path: %s", path_name(p));

	if (p->state != STATE_STOPPING)
		p->state = STATE_STOPPING;

	/* Cancel the thread in case is currently in a blocking syscall */
	ret = pthread_cancel(p->tid);
	if (ret)
		return ret;

	ret = pthread_join(p->tid, NULL);
	if (ret)
		return ret;

#ifdef WITH_HOOKS
	ret = hook_list_stop(&p->hooks);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	sample_decref(p->last_sample);

	p->state = STATE_STOPPED;

	return 0;
}

int path_destroy(struct path *p)
{
	int ret;

	if (p->state == STATE_DESTROYED)
		return 0;

#ifdef WITH_HOOKS
	ret = hook_list_destroy(&p->hooks);
	if (ret)
		return ret;
#endif
	ret = signal_list_destroy(&p->signals);
	if (ret)
		return ret;

	ret = vlist_destroy(&p->sources, (dtor_cb_t) path_source_destroy, true);
	if (ret)
		return ret;

	ret = vlist_destroy(&p->destinations, (dtor_cb_t) path_destination_destroy, true);
	if (ret)
		return ret;

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

bool path_is_simple(const struct path *p)
{
	int ret;
	const char *in = NULL, *out = NULL;

	json_error_t err;
	ret = json_unpack_ex(p->cfg, &err, 0, "{ s: s, s: s }", "in", &in, "out", &out);
	if (ret)
		return false;

	ret = node_is_valid_name(in);
	if (!ret)
		return false;

	ret = node_is_valid_name(out);
	if (!ret)
		return false;

	return true;
}

bool path_is_enabled(const struct path *p)
{
	return p->enabled;
}

bool path_is_reversed(const struct path *p)
{
	return p->reverse;
}

struct vlist * path_get_signals(struct path *p)
{
	return &p->signals;
}
