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

#include <algorithm>
#include <list>
#include <map>

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
#include <villas/memory.h>
#include <villas/node.h>
#include <villas/signal.h>
#include <villas/path.h>
#include <villas/kernel/rt.hpp>
#include <villas/path_source.h>
#include <villas/path_destination.h>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

/** Main thread function per path:
 *     read samples from source -> write samples to destinations
 *
 * This is an optimized version of path_run_poll() which is
 * used for paths which only have a single source.
 * In this case we case save a call to poll() and directly call
 * path_source_read() / node_read().
 */
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

/** Main thread function per path:
 *     read samples from source -> write samples to destinations
 *
 * This variant of the path uses poll() to listen on an event from
 * all path sources.
 */
static void * path_run_poll(void *arg)
{
	int ret;
	struct vpath *p = (struct vpath *) arg;

	while (p->state == State::STARTED) {
		ret = poll(p->reader.pfds, p->reader.nfds, -1);
		if (ret < 0)
			throw SystemError("Failed to poll");

		p->logger->debug("Path {} returned from poll(2)", path_name(p));

		for (int i = 0; i < p->reader.nfds; i++) {
			struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

			if (p->reader.pfds[i].revents & POLLIN) {
				/* Timeout: re-enqueue the last sample */
				if (p->reader.pfds[i].fd == p->timeout.getFD()) {
					p->timeout.wait();

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
	new (&p->timeout) Task(CLOCK_MONOTONIC);

	static int path_id;
	auto logger_name = fmt::format("path:{}", path_id++);
	p->logger = logging.get(logger_name);

	uuid_clear(p->uuid);

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
	p->affinity = 0;

	p->state = State::INITIALIZED;

	return 0;
}

static int path_prepare_poll(struct vpath *p)
{
	int fds[16], n = 0, m;

	if (p->reader.pfds)
		delete[] p->reader.pfds;

	p->reader.pfds = nullptr;
	p->reader.nfds = 0;

	for (unsigned i = 0; i < vlist_length(&p->sources); i++) {
		struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

		m = node_poll_fds(ps->node, fds);
		if (m <= 0)
			throw RuntimeError("Failed to get file descriptor for node {}", node_name(ps->node));

		p->reader.nfds += m;
		p->reader.pfds = (struct pollfd *) realloc(p->reader.pfds, p->reader.nfds * sizeof(struct pollfd));

		for (int i = 0; i < m; i++) {
			if (fds[i] < 0)
				throw RuntimeError("Failed to get file descriptor for node {}", node_name(ps->node));

			/* This slot is only used if it is not masked */
			p->reader.pfds[n].events = POLLIN;
			p->reader.pfds[n++].fd = fds[i];
		}
	}

	/* We use the last slot for the timeout timer. */
	if (p->rate > 0) {
		p->timeout.setRate(p->rate);

		p->reader.nfds++;
		p->reader.pfds = (struct pollfd *) realloc(p->reader.pfds, p->reader.nfds * sizeof(struct pollfd));

		p->reader.pfds[p->reader.nfds-1].events = POLLIN;
		p->reader.pfds[p->reader.nfds-1].fd = p->timeout.getFD();
		if (p->reader.pfds[p->reader.nfds-1].fd < 0) {
			p->logger->warn("Failed to get file descriptor for timer of path {}", path_name(p));
			return -1;
		}
	}

	return 0;
}

int path_prepare(struct vpath *p, NodeList &nodes)
{
	int ret;
	unsigned pool_size;

	struct memory_type *pool_mt = memory_default;

	assert(p->state == State::CHECKED);

	p->mask.reset();

	/* Prepare mappings */
	ret = mapping_list_prepare(&p->mappings, nodes);
	if (ret)
		return ret;

	p->muxed = path_is_muxed(p);

	/* Create path sources */
	std::map<struct vnode *, struct vpath_source *> pss;
	for (size_t i = 0; i < vlist_length(&p->mappings); i++) {
		struct mapping_entry *me = (struct mapping_entry *) vlist_at(&p->mappings, i);
		struct vnode *n = me->node;
		struct vpath_source *ps;

		if (pss.find(n) != pss.end())
			/* We already have a path source for this mapping entry */
			ps = pss[n];
		else {
			/* Create new path source */
			ps = pss[n] = new struct vpath_source;
			if (!ps)
				throw MemoryAllocationError();

			/* Depending on weather the node belonging to this mapping is already
			 * used by another path or not, we will create a master or secondary
			 * path source.
			 * A secondary path source uses an internal loopback node / queue
			 * to forward samples from on path to another.
			 */
			bool isSecondary = vlist_length(&n->sources) > 0;
			ret = isSecondary
				? path_source_init_secondary(ps, n)
				: path_source_init_master(ps, n);
			if (ret)
				return ret;

			if (ps->type == PathSourceType::SECONDARY) {
				nodes.push_back(ps->node);
				vlist_push(&ps->node->sources, ps);
			}

			if (p->mask_list.empty() || std::find(p->mask_list.begin(), p->mask_list.end(), n) != p->mask_list.end()) {
				ps->masked = true;
				p->mask.set(i);
			}

			vlist_push(&n->sources, ps);
			vlist_push(&p->sources, ps);
		}

		struct vlist *sigs = node_input_signals(me->node);

		/* Update signals of path */
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
				if (!sig)
					throw MemoryAllocationError();

				ret = signal_init_from_mapping(sig, me, j);
				if (ret)
					return -1;
			}

			vlist_extend(&p->signals, me->offset + j + 1, nullptr);
			vlist_set(&p->signals, me->offset + j, sig);
		}

		vlist_push(&ps->mappings, me);
	}

	/* Prepare path destinations */
	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		auto *pd = (struct vpath_destination *) vlist_at(&p->destinations, i);

		if (node_type(pd->node)->pool_size > pool_size)
			pool_size = node_type(pd->node)->pool_size;

		if (node_type(pd->node)->memory_type)
			pool_mt = node_memory_type(pd->node);

		ret = path_destination_prepare(pd, p->queuelen);
		if (ret)
			return ret;
	}

	/* Autodetect whether to use original sequence numbers or not */
	if (p->original_sequence_no == -1)
		p->original_sequence_no = vlist_length(&p->sources) == 1;

	/* Autodetect whether to use poll() for this path or not */
	if (p->poll == -1) {
		if (p->rate > 0)
			p->poll = 1;
		else if (vlist_length(&p->sources) > 1)
			p->poll = 1;
		else
			p->poll = 0;
	}

	/* Prepare poll() */
	if (p->poll) {
		ret = path_prepare_poll(p);
		if (ret)
			return ret;
	}

#ifdef WITH_HOOKS
	/* Prepare path hooks */
	int m = p->builtin ? (int) Hook::Flags::PATH | (int) Hook::Flags::BUILTIN : 0;

	/* Add internal hooks if they are not already in the list */
	hook_list_prepare(&p->hooks, &p->signals, m, p, nullptr);
#endif /* WITH_HOOKS */

	/* Prepare pool */
	pool_size = MAX(1UL, vlist_length(&p->destinations)) * p->queuelen;
	ret = pool_init(&p->pool, pool_size, SAMPLE_LENGTH(vlist_length(&p->signals)), pool_mt);

	if (ret)
		return ret;

	p->logger->info("Prepared path {} with {} output signals", path_name(p), vlist_length(path_output_signals(p)));
	signal_list_dump(p->logger, path_output_signals(p));

	p->state = State::PREPARED;

	return 0;
}

int path_parse(struct vpath *p, json_t *json, NodeList &nodes, const uuid_t sn_uuid)
{
	int ret;

	json_error_t err;
	json_t *json_in;
	json_t *json_out = nullptr;
	json_t *json_hooks = nullptr;
	json_t *json_mask = nullptr;

	const char *mode = nullptr;
	const char *uuid_str = nullptr;

	struct vlist destinations;

	ret = vlist_init(&destinations);
	if (ret)
		return ret;

	ret = json_unpack_ex(json, &err, 0, "{ s: o, s?: o, s?: o, s?: b, s?: b, s?: b, s?: i, s?: s, s?: b, s?: F, s?: o, s?: b, s?: s, s?: i }",
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
		"original_sequence_no", &p->original_sequence_no,
		"uuid", &uuid_str,
		"affinity", &p->affinity
	);
	if (ret)
		throw ConfigError(json, err, "node-config-path", "Failed to parse path configuration");

	/* Optional settings */
	if (mode) {
		if      (!strcmp(mode, "any"))
			p->mode = PathMode::ANY;
		else if (!strcmp(mode, "all"))
			p->mode = PathMode::ALL;
		else
			throw ConfigError(json, "node-config-path", "Invalid path mode '{}'", mode);
	}

	/* UUID */
	if (uuid_str) {
		ret = uuid_parse(uuid_str, p->uuid);
		if (ret)
			throw ConfigError(json, "node-config-path-uuid", "Failed to parse UUID: {}", uuid_str);
	}
	else
		/* Generate UUID from hashed config */
		uuid_generate_from_json(p->uuid, json, sn_uuid);

	/* Input node(s) */
	ret = mapping_list_parse(&p->mappings, json_in);
	if (ret)
		throw ConfigError(json_in, "node-config-path-in", "Failed to parse input mapping of path {}", path_name(p));

	/* Output node(s) */
	if (json_out) {
		ret = node_list_parse(&destinations, json_out, nodes);
		if (ret)
			throw ConfigError(json_out, "node-config-path-out", "Failed to parse output nodes");
	}

	for (size_t i = 0; i < vlist_length(&destinations); i++) {
		struct vnode *n = (struct vnode *) vlist_at(&destinations, i);

		if (n->out.path)
			throw ConfigError(json, "node-config-path", "Every node must only be used by a single path as destination");

		n->out.path = p;

		auto *pd = new struct vpath_destination;
		if (!pd)
			throw MemoryAllocationError();

		ret = path_destination_init(pd, n);
		if (ret)
			return ret;

		vlist_push(&n->destinations, pd);
		vlist_push(&p->destinations, pd);
	}

#ifdef WITH_HOOKS
	if (json_hooks)
		hook_list_parse(&p->hooks, json_hooks, (int) Hook::Flags::PATH, p, nullptr);
#endif /* WITH_HOOKS */

	if (json_mask)
		path_parse_mask(p, json_mask, nodes);

	ret = vlist_destroy(&destinations, nullptr, false);
	if (ret)
		return ret;

	p->config = json;
	p->state = State::PARSED;

	return 0;
}

void path_parse_mask(struct vpath *p, json_t *json_mask, villas::node::NodeList &nodes)
{
	json_t *json_entry;
	size_t i;

	if (!json_is_array(json_mask))
		throw ConfigError(json_mask, "node-config-path-mask", "The 'mask' setting must be a list of node names");

	json_array_foreach(json_mask, i, json_entry) {
		const char *name;
		struct vnode *node;

		name = json_string_value(json_entry);
		if (!name)
			throw ConfigError(json_mask, "node-config-path-mask", "The 'mask' setting must be a list of node names");

		node = nodes.lookup(name);
		if (!node)
			throw ConfigError(json_mask, "node-config-path-mask", "The 'mask' entry '{}' is not a valid node name", name);

		p->mask_list.push_back(node);
	}
}

void path_check(struct vpath *p)
{
	assert(p->state != State::DESTROYED);

	if (p->rate < 0)
		throw RuntimeError("Setting 'rate' of path {} must be a positive number.", path_name(p));

	if (p->poll > 0) {
		if (p->rate <= 0) {
			/* Check that all path sources provide a file descriptor for polling */
			for (size_t i = 0; i < vlist_length(&p->sources); i++) {
				struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

				if (!node_type(ps->node)->poll_fds)
					throw RuntimeError("Node {} can not be used in polling mode with path {}", node_name(ps->node), path_name(p));
			}
		}
	}
	else {
		/* Check that we do not need to multiplex between multiple sources when polling is disabled */
		if (vlist_length(&p->sources) > 1)
			throw RuntimeError("Setting 'poll' must be active if the path has more than one source");

		/* Check that we do not use the fixed rate feature when polling is disabled */
		if (p->rate > 0)
			throw RuntimeError("Setting 'poll' must be activated when used together with setting 'rate'");
	}

	if (!IS_POW2(p->queuelen)) {
		p->queuelen = LOG2_CEIL(p->queuelen);
		p->logger->warn("Queue length should always be a power of 2. Adjusting to {}", p->queuelen);
	}

	for (size_t i = 0; i < vlist_length(&p->sources); i++) {
		struct vpath_source *ps = (struct vpath_source *) vlist_at(&p->sources, i);

		path_source_check(ps);
	}

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct vpath_destination *ps = (struct vpath_destination *) vlist_at(&p->destinations, i);

		path_destination_check(ps);
	}

	p->state = State::CHECKED;
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

	p->logger->info("Starting path {}: #signals={}({}), #hooks={}, #sources={}, "
	                "#destinations={}, mode={}, poll={}, mask={:b}, rate={}, "
	                "enabled={}, reversed={}, queuelen={}, original_sequence_no={}",
		path_name(p),
		vlist_length(&p->signals),
		vlist_length(path_output_signals(p)),
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

	if (p->affinity)
		kernel::rt::setThreadAffinity(p->tid, p->affinity);

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

	ret = vlist_destroy(&p->mappings, (dtor_cb_t) mapping_entry_destroy, true);
	if (ret)
		return ret;

	if (p->reader.pfds)
		delete[] p->reader.pfds;

	if (p->_name)
		free(p->_name);

	ret = pool_destroy(&p->pool);
	if (ret)
		return ret;

	using bs = std::bitset<MAX_SAMPLE_LENGTH>;
	using lg = std::shared_ptr<spdlog::logger>;

	p->received.~bs();
	p->mask.~bs();
	p->logger.~lg();
	p->timeout.~Task();

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

bool path_is_simple(const struct vpath *p)
{
	int ret;
	const char *in = nullptr, *out = nullptr;

	json_error_t err;
	ret = json_unpack_ex(p->config, &err, 0, "{ s: s, s: s }", "in", &in, "out", &out);
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

bool path_is_muxed(const struct vpath *p)
{
	if (vlist_length(&p->sources) > 0)
		return true;

	if (vlist_length(&p->mappings) > 0)
		return true;

	struct mapping_entry *me = (struct mapping_entry *) vlist_at_safe(&p->mappings, 0);

	if (me->type != MappingType::DATA)
		return true;

	if (me->data.offset != 0)
		return true;

	if (me->length != -1)
		return true;

	return false;
}

bool path_is_enabled(const struct vpath *p)
{
	return p->enabled;
}

bool path_is_reversed(const struct vpath *p)
{
	return p->reverse;
}

struct vlist * path_output_signals(struct vpath *p)
{
#ifdef WITH_HOOKS
	if (vlist_length(&p->hooks) > 0)
		return hook_list_get_signals(&p->hooks);
#endif /* WITH_HOOKS */

	return &p->signals;
}

json_t * path_to_json(struct vpath *p)
{
	char uuid[37];
	uuid_unparse(p->uuid, uuid);

	json_t *json_signals = signal_list_to_json(&p->signals);
	json_t *json_hooks = hook_list_to_json(&p->hooks);
	json_t *json_sources = json_array();
	json_t *json_destinations = json_array();

	for (size_t i = 0; i < vlist_length(&p->sources); i++) {
		struct vpath_source *pd = (struct vpath_source *) vlist_at_safe(&p->sources, i);

		json_array_append_new(json_sources, json_string(node_name_short(pd->node)));
	}

	for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
		struct vpath_destination *pd = (struct vpath_destination *) vlist_at_safe(&p->destinations, i);

		json_array_append_new(json_destinations, json_string(node_name_short(pd->node)));
	}

	json_t *json_path = json_pack("{ s: s, s: s, s: s, s: b, s: b s: b, s: b, s: b, s: b s: i, s: o, s: o, s: o, s: o }",
		"uuid", uuid,
		"state", state_print(p->state),
		"mode", p->mode == PathMode::ANY ? "any" : "all",
		"enabled", p->enabled,
		"builtin", p->builtin,
		"reverse", p->reverse,
		"original_sequence_no", p->original_sequence_no,
		"last_sequence", p->last_sequence,
		"poll", p->poll,
		"queuelen", p->queuelen,
		"signals", json_signals,
		"hooks", json_hooks,
		"in", json_sources,
		"out", json_destinations
	);

	return json_path;
}
