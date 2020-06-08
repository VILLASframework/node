/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdint>
#include <cstring>
#include <cinttypes>
#include <cerrno>

#include <unistd.h>
#include <poll.h>

#include <villas/node/config.h>
#include <villas/utils.hpp>
#include <villas/colors.hpp>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/queue.h>
#include <villas/hook.hpp>
#include <villas/hook_list.hpp>
#include <villas/plugin.h>
#include <villas/memory.h>
#include <villas/node.h>
#include <villas/signal.h>
#include <villas/path.h>
#include <villas/path_source.h>
#include <villas/path_destination.h>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static void * path_run_single(void *arg)
{
	int ret;
	struct vpath *p = (struct vpath *) arg;
	struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, 0);

	while (p->state == State::STARTED) {
		pthread_testcancel();

		ret = path_source_read(ps, p, 0);
		if (ret <= 0)
			continue;

		for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
			struct vpath_destination *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

			path_destination_write(pd, p);
		}
	}

	return nullptr;
}

/** Main thread function per path: read samples -> write samples */
static void * path_run_poll(void *arg)
{
	int ret;
	struct vpath *p = (struct vpath *) arg;

	while (p->state == State::STARTED) {
		ret = poll(p->reader.pfds, p->reader.nfds, -1);
		if (ret < 0)
			serror("Failed to poll");

		p->logger->debug("Path {} returned from poll(2)", path_name(p));

		for (int i = 0; i < p->reader.nfds; i++) {
			struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

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
			struct vpath_destination *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

			path_destination_write(pd, p);
		}
	}

	return nullptr;
}

int path_init(struct vpath *p)
{
	int ret;

	new (&p->logger) Logger;
	new (&p->received) std::bitset<MAX_SAMPLE_LENGTH>;
	new (&p->mask) std::bitset<MAX_SAMPLE_LENGTH>;

	p->logger = logging.get("path");

	ret = vlist_init(&p->destinations);
	if (ret)
		return ret;

	ret = vlist_init(&p->sources);
	if (ret)
		return ret;

	ret = signal_list_init(&p->signals);
	if (ret)
		return ret;

	ret = vlist_init(&p->mappings);
	if (ret)
		return ret;

#ifdef WITH_HOOKS
	ret = hook_list_init(&p->hooks);
	if (ret)
		return ret;
#endif /* WITH_HOOKS */

	p->_name = nullptr;

	p->reader.pfds = nullptr;
	p->reader.nfds = 0;

	/* Default values */
	p->mode = PathMode::ANY;
	p->rate = 0; /* Disabled */

	p->builtin = 1;
	p->reverse = 0;
	p->enabled = 1;
	p->poll = -1;
	p->queuelen = DEFAULT_QUEUE_LENGTH;
	p->original_sequence_no = -1;

	p->state = State::INITIALIZED;

	return 0;
}

static int path_prepare_poll(struct vpath *p)
{
	int fds[16], ret, n = 0, m;

	if (p->reader.pfds)
		delete[] p->reader.pfds;

	p->reader.pfds = nullptr;
	p->reader.nfds = 0;

	for (unsigned i = 0; i < vlist_length(&p->sources); i++) {
		struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

		m = node_poll_fds(ps->node, fds);
		if (m < 0)
			continue;

		p->reader.nfds += m;
		p->reader.pfds = (struct pollfd *) realloc(p->reader.pfds, p->reader.nfds * sizeof(struct pollfd));

		for (int i = 0; i < m; i++) {
			if (fds[i] < 0) {
				p->logger->error("Failed to get file descriptor for node {}", node_name(ps->node));
				return -1;
			}

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
		p->reader.pfds = (struct pollfd *) realloc(p->reader.pfds, p->reader.nfds * sizeof(struct pollfd));

		p->reader.pfds[p->reader.nfds-1].events = POLLIN;
		p->reader.pfds[p->reader.nfds-1].fd = task_fd(&p->timeout);
		if (p->reader.pfds[p->reader.nfds-1].fd < 0) {
			p->logger->warn("Failed to get file descriptor for timer of path {}", path_name(p));
			return -1;
		}
	}

	return 0;
}

int path_prepare(struct vpath *p)
{
	int ret;

	assert(p->state == State::CHECKED);

	/* Initialize destinations */
	struct memory_type *pool_mt = memory_default;
	unsigned pool_size = MAX(1UL, vlist_length(&p->destinations)) * p->queuelen;

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct vpath_destination *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

		if (node_type(pd->node)->pool_size > pool_size)
			pool_size = node_type(pd->node)->pool_size;

		if (node_type(pd->node)->memory_type)
			pool_mt = node_memory_type(pd->node);

		ret = path_destination_init(pd, p->queuelen);
		if (ret)
			return ret;
	}

	/* Initialize sources */
	ret = mapping_list_prepare(&p->mappings);
	if (ret)
		return ret;

	for (size_t i = 0; i < vlist_length(&p->sources); i++) {
		struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

		ret = path_source_init(ps);
		if (ret)
			return ret;

		if (ps->masked)
			p->mask.set(i);

		for (size_t i = 0; i < vlist_length(&ps->mappings); i++) {
			struct mapping_entry *me = (struct mapping_entry *) vlist_at(&ps->mappings, i);
			struct vlist *sigs = node_get_signals(me->node, NodeDir::IN);

			for (unsigned j = 0; j < (unsigned) me->length; j++) {
				struct signal *sig;

				/* For data mappings we simple refer to the existing
				 * signal descriptors of the source node. */
				if (me->type == MappingType::DATA) {
					sig = (struct signal *) vlist_at_safe(sigs, me->data.offset + j);
					if (!sig) {
						p->logger->warn("Failed to create signal description for path {}", path_name(p));
						continue;
					}

					signal_incref(sig);
				}
				/* For other mappings we create new signal descriptors */
				else {
					sig = new struct signal;

					ret = signal_init_from_mapping(sig, me, j);
					if (ret)
						return -1;
				}

				vlist_extend(&p->signals, me->offset + j + 1, nullptr);
				vlist_set(&p->signals, me->offset + j, sig);
			}
		}
	}

#ifdef WITH_HOOKS
	int m = p->builtin ? (int) Hook::Flags::PATH | (int) Hook::Flags::BUILTIN : 0;

	/* Add internal hooks if they are not already in the list */
	hook_list_prepare(&p->hooks, &p->signals, m, p, nullptr);
#endif /* WITH_HOOKS */

	/* Initialize pool */
	ret = pool_init(&p->pool, pool_size, SAMPLE_LENGTH(vlist_length(&p->signals)), pool_mt);
	if (ret)
		return ret;

	if (p->original_sequence_no == -1)
		p->original_sequence_no = vlist_length(&p->sources) == 1;

	p->state = State::PREPARED;

	return 0;
}

int path_parse(struct vpath *p, json_t *cfg, struct vlist *nodes)
{
	int ret;

	json_error_t err;
	json_t *json_in;
	json_t *json_out = nullptr;
	json_t *json_hooks = nullptr;
	json_t *json_mask = nullptr;

	const char *mode = nullptr;

	struct vlist destinations = { .state = State::DESTROYED };

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
	ret = mapping_list_parse(&p->mappings, json_in, nodes);
	if (ret) {
		p->logger->error("Failed to parse input mapping of path {}", path_name(p));
		return -1;
	}

	/* Optional settings */
	if (mode) {
		if      (!strcmp(mode, "any"))
			p->mode = PathMode::ANY;
		else if (!strcmp(mode, "all"))
			p->mode = PathMode::ALL;
		else {
			p->logger->error("Invalid path mode '{}'", mode);
			return -1;
		}
	}

	/* Output node(s) */
	if (json_out) {
		ret = node_list_parse(&destinations, json_out, nodes);
		if (ret)
			jerror(&err, "Failed to parse output nodes");
	}

	for (size_t i = 0; i < vlist_length(&p->mappings); i++) {
		struct mapping_entry *me = (struct mapping_entry *) vlist_at(&p->mappings, i);
		struct vpath_source *ps = nullptr;

		/* Check if there is already a path_source for this source */
		for (size_t j = 0; j < vlist_length(&p->sources); j++) {
			struct vpath_source *pt = (struct vpath_source *) vlist_at(&p->sources, j);

			if (pt->node == me->node) {
				ps = pt;
				break;
			}
		}

		/* Create new path_source of not existing */
		if (!ps) {
			ps = new struct vpath_source;

			ps->node = me->node;
			ps->masked = false;

			ps->mappings.state = State::DESTROYED;

			vlist_init(&ps->mappings);

			vlist_push(&p->sources, ps);
		}

		if (!node_is_enabled(ps->node)) {
			p->logger->error("Source {} of path {} is not enabled", node_name(ps->node), path_name(p));
			return -1;
		}

		vlist_push(&ps->mappings, me);
	}

	for (size_t i = 0; i < vlist_length(&destinations); i++) {
		struct node *n = (struct node *) vlist_at(&destinations, i);

		auto *pd = new struct vpath_destination;

		pd->node = n;

		if (!node_is_enabled(pd->node)) {
			p->logger->error("Destination {} of path {} is not enabled", node_name(pd->node), path_name(p));
			return -1;
		}

		vlist_push(&p->destinations, pd);
	}

	if (json_mask) {
		json_t *json_entry;
		size_t i;

		if (!json_is_array(json_mask)) {
			p->logger->error("The 'mask' setting must be a list of node names");
			return -1;
		}

		json_array_foreach(json_mask, i, json_entry) {
			const char *name;
			struct node *node;
			struct vpath_source *ps = nullptr;

			name = json_string_value(json_entry);
			if (!name) {
				p->logger->error("The 'mask' setting must be a list of node names");
				return -1;
			}

			node = (struct node *) vlist_lookup(nodes, name);
			if (!node) {
				p->logger->error("The 'mask' entry '{}' is not a valid node name", name);
				return -1;
			}

			/* Search correspondending path_source to node */
			for (size_t i = 0; i < vlist_length(&p->sources); i++) {
				struct vpath_source *pt = (struct vpath_source *) vlist_at(&p->sources, i);

				if (pt->node == node) {
					ps = pt;
					break;
				}
			}

			if (!ps) {
				p->logger->error("Node {} is not a source of the path {}", node_name(node), path_name(p));
				return -1;
			}

			ps->masked = true;
		}
	}
	/* Enable all by default */
	else {
		for (size_t i = 0; i < vlist_length(&p->sources); i++) {
			struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

			ps->masked = true;
		}
	}

#ifdef WITH_HOOKS
	if (json_hooks) {
		hook_list_parse(&p->hooks, json_hooks, (int) Hook::Flags::PATH, p, nullptr);
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

	ret = vlist_destroy(&destinations, nullptr, false);
	if (ret)
		return ret;

	p->cfg = cfg;
	p->state = State::PARSED;

	return 0;
}

int path_check(struct vpath *p)
{
	assert(p->state != State::DESTROYED);

	if (p->rate < 0) {
		p->logger->error("Setting 'rate' of path {} must be a positive number.", path_name(p));
		return -1;
	}

	if (p->poll) {
		if (p->rate <= 0) {
			/* Check that all path sources provide a file descriptor for polling */
			for (size_t i = 0; i < vlist_length(&p->sources); i++) {
				struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

				if (!node_type(ps->node)->poll_fds) {
					p->logger->error("Node {} can not be used in polling mode with path {}", node_name(ps->node), path_name(p));
					return -1;
				}
			}
		}
	}
	else {
		/* Check that we do not need to multiplex between multiple sources when polling is disabled */
		if (vlist_length(&p->sources) > 1) {
			p->logger->error("Setting 'poll' must be active if the path has more than one source");
			return -1;
		}

		/* Check that we do not use the fixed rate feature when polling is disabled */
		if (p->rate > 0) {
			p->logger->error("Setting 'poll' must be activated when used together with setting 'rate'");
			return -1;
		}
	}

	for (size_t i = 0; i < vlist_length(&p->sources); i++) {
		struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

		if (!node_type(ps->node)->read) {
			p->logger->error("Node {} is not supported as a source for path {}", node_name(ps->node), path_name(p));
			return -1;
		}
	}

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct vpath_destination *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

		if (!node_type(pd->node)->write) {
			p->logger->error("Destiation node {} is not supported as a sink for path ", node_name(pd->node), path_name(p));
			return -1;
		}
	}

	if (!IS_POW2(p->queuelen)) {
		p->queuelen = LOG2_CEIL(p->queuelen);
		p->logger->warn("Queue length should always be a power of 2. Adjusting to {}", p->queuelen);
	}

	p->state = State::CHECKED;

	return 0;
}

int path_start(struct vpath *p)
{
	int ret;
	const char *mode;

	assert(p->state == State::PREPARED);

	switch (p->mode) {
		case PathMode::ANY:
			mode = "any";
			break;

		case PathMode::ALL:
			mode = "all";
			break;

		default:
			mode = "unknown";
			break;
	}

	p->logger->info("Starting path {}: #signals={}, #hooks={}, #sources={}, "
	                "#destinations={}, mode={}, poll={}, mask={:b}, rate={}, "
					"enabled={}, reversed={}, queuelen={}, original_sequence_no={}",
		path_name(p),
		vlist_length(&p->signals),
		vlist_length(&p->hooks),
		vlist_length(&p->sources),
		vlist_length(&p->destinations),
		mode,
		p->poll ? "yes" : "no",
		p->mask.to_ullong(),
		p->rate,
		path_is_enabled(p) ? "yes" : "no",
		path_is_reversed(p) ? "yes" : "no",
		p->queuelen,
		p->original_sequence_no ? "yes" : "no"
	);

#ifdef WITH_HOOKS
	hook_list_start(&p->hooks);
#endif /* WITH_HOOKS */

	p->last_sequence = 0;

	p->received.reset();

	/* We initialize the intial sample */
	p->last_sample = sample_alloc(&p->pool);
	if (!p->last_sample)
		return -1;

	p->last_sample->length = 0;
	p->last_sample->signals = &p->signals;
	p->last_sample->sequence = 0;
	p->last_sample->flags = p->last_sample->length > 0 ? (int) SampleFlags::HAS_DATA : 0;

	for (size_t i = 0; i < p->last_sample->length; i++) {
		struct signal *sig = (struct signal *) vlist_at(p->last_sample->signals, i);

		p->last_sample->data[i] = sig->init;
	}

	/* Prepare poll() */
	if (p->poll) {
		ret = path_prepare_poll(p);
		if (ret)
			return ret;
	}

	p->state = State::STARTED;

	/* Start one thread per path for sending to destinations
	 *
	 * Special case: If the path only has a single source and this source
	 * does not offer a file descriptor for polling, we will use a special
	 * thread function.
	 */
	ret = pthread_create(&p->tid, nullptr, p->poll ? path_run_poll : path_run_single, p);
	if (ret)
		return ret;

	return 0;
}

int path_stop(struct vpath *p)
{
	int ret;

	if (p->state != State::STARTED && p->state != State::STOPPING)
		return 0;

	p->logger->info("Stopping path: {}", path_name(p));

	if (p->state != State::STOPPING)
		p->state = State::STOPPING;

	/* Cancel the thread in case is currently in a blocking syscall.
	 *
	 * We dont care if the thread has already been terminated.
	 */
	ret = pthread_cancel(p->tid);
	if (ret && ret != ESRCH)
	 	return ret;

	ret = pthread_join(p->tid, nullptr);
	if (ret)
		return ret;

#ifdef WITH_HOOKS
	hook_list_stop(&p->hooks);
#endif /* WITH_HOOKS */

	sample_decref(p->last_sample);

	p->state = State::STOPPED;

	return 0;
}

int path_destroy(struct vpath *p)
{
	int ret;

	if (p->state == State::DESTROYED)
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

	ret = vlist_destroy(&p->mappings, nullptr, true);
	if (ret)
		return ret;

	if (p->reader.pfds)
		delete[] p->reader.pfds;

	if (p->_name)
		free(p->_name);

	if (p->rate > 0)
		task_destroy(&p->timeout);

	ret = pool_destroy(&p->pool);
	if (ret)
		return ret;

	using bs = std::bitset<MAX_SAMPLE_LENGTH>;
	using lg = std::shared_ptr<spdlog::logger>;

	p->received.~bs();
	p->mask.~bs();
	p->logger.~lg();

	p->state = State::DESTROYED;

	return 0;
}

const char * path_name(struct vpath *p)
{
	if (!p->_name) {
		strcatf(&p->_name, "[");

		for (size_t i = 0; i < vlist_length(&p->sources); i++) {
			struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

			strcatf(&p->_name, " %s", node_name_short(ps->node));
		}

		strcatf(&p->_name, " ] " CLR_MAG("=>") " [");

		for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
			struct vpath_destination *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

			strcatf(&p->_name, " %s", node_name_short(pd->node));
		}

		strcatf(&p->_name, " ]");
	}

	return p->_name;
}

int path_uses_node(struct vpath *p, struct node *n)
{
	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct vpath_destination *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

		if (pd->node == n)
			return 0;
	}

	for (size_t i = 0; i < vlist_length(&p->sources); i++) {
		struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

		if (ps->node == n)
			return 0;
	}

	return -1;
}

bool path_is_simple(const struct vpath *p)
{
	int ret;
	const char *in = nullptr, *out = nullptr;

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

bool path_is_enabled(const struct vpath *p)
{
	return p->enabled;
}

bool path_is_reversed(const struct vpath *p)
{
	return p->reverse;
}

struct vlist * path_get_signals(struct vpath *p)
{
	return &p->signals;
}
