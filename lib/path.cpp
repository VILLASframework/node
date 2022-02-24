/** Message paths.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
#include <iterator>

#include <unistd.h>
#include <poll.h>

#include <villas/node/config.hpp>
#include <villas/utils.hpp>
#include <villas/colors.hpp>
#include <villas/uuid.hpp>
#include <villas/timing.hpp>
#include <villas/pool.hpp>
#include <villas/queue.h>
#include <villas/hook.hpp>
#include <villas/hook_list.hpp>
#include <villas/node/memory.hpp>
#include <villas/node.hpp>
#include <villas/signal.hpp>
#include <villas/path.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/path_source.hpp>
#include <villas/path_destination.hpp>

using namespace villas;
using namespace villas::node;

void * Path::runWrapper(void *arg)
{
	auto *p = (Path *) arg;

	return p->poll
		? p->runPoll()
		: p->runSingle();
}

/** Main thread function per path:
 *     read samples from source -> write samples to destinations
 *
 * This is an optimized version of runPoll() which is
 * used for paths which only have a single source.
 * In this case we case save a call to poll() and directly call
 * PathSource::read() / Node::read().
 */
void * Path::runSingle()
{
	int ret;
	auto ps = sources.front();  /* there is only a single source */

	while (state == State::STARTED) {
		pthread_testcancel();

		ret = ps->read(0);
		if (ret <= 0)
			continue;

		for (auto pd : destinations)
			pd->write();
	}

	return nullptr;
}

/** Main thread function per path:
 *     read samples from source -> write samples to destinations
 *
 * This variant of the path uses poll() to listen on an event from
 * all path sources.
 */
void * Path::runPoll()
{
	while (state == State::STARTED) {
		int ret = ::poll(pfds.data(), pfds.size(), -1);
		if (ret < 0)
			throw SystemError("Failed to poll");

		logger->debug("returned from poll(2): ret={}", ret);

		for (unsigned i = 0; i < pfds.size(); i++) {
			auto &pfd = pfds[i];

			if (pfd.revents & POLLIN) {
				/* Timeout: re-enqueue the last sample */
				if (pfd.fd == timeout.getFD()) {
					timeout.wait();

					last_sample->sequence = last_sequence++;

					PathDestination::enqueueAll(this, &last_sample, 1);
				}
				/* A source is ready to receive samples */
				else {
					auto ps = sources[i];

					ps->read(i);
				}
			}
		}

		for (auto pd : destinations)
			pd->write();
	}

	return nullptr;
}

Path::Path() :
	state(State::INITIALIZED),
	mode(Mode::ANY),
	timeout(CLOCK_MONOTONIC),
	rate(0), /* Disabled */
	affinity(0),
	enabled(true),
	poll(-1),
	reversed(false),
	builtin(true),
	original_sequence_no(-1),
	queuelen(DEFAULT_QUEUE_LENGTH),
	logger(logging.get(fmt::format("path:{}", id++)))
{
	uuid_clear(uuid);

	pool.state = State::DESTROYED;
}

void Path::startPoll()
{
	pfds.clear();

	for (auto ps : sources) {
		auto fds = ps->getNode()->getPollFDs();
		for (auto fd : fds) {
			if (fd < 0)
				throw RuntimeError("Failed to get file descriptor for node {}", *ps->getNode());

			/* This slot is only used if it is not masked */
			struct pollfd pfd = {
				.fd = fd,
				.events = POLLIN
			};

			pfds.push_back(pfd);
		}
	}

	/* We use the last slot for the timeout timer. */
	if (rate > 0) {
		timeout.setRate(rate);

		struct pollfd pfd = {
			.fd = timeout.getFD(),
			.events = POLLIN
		};

		if (pfd.fd < 0)
			throw RuntimeError("Failed to get file descriptor for timer of path {}", *this);

		pfds.push_back(pfd);
	}
}

void Path::prepare(NodeList &nodes)
{
	int ret;

	struct memory::Type *pool_mt = memory::default_type;

	assert(state == State::CHECKED);

	mask.reset();
	signals = std::make_shared<SignalList>();

	/* Prepare mappings */
	ret = mappings.prepare(nodes);
	if (ret)
		throw RuntimeError("Failed to prepare mappings of path: {}", *this);

	/* Create path sources */
	std::map<Node *, PathSource::Ptr> psm;
	unsigned i = 0, j = 0;
	for (auto me : mappings) {
		Node *n = me->node;
		PathSource::Ptr ps;

		if (psm.find(n) != psm.end())
			/* We already have a path source for this mapping entry */
			ps = psm[n];
		else {
			/* Depending on weather the node belonging to this mapping is already
			 * used by another path or not, we will create a master or secondary
			 * path source.
			 * A secondary path source uses an internal loopback node / queue
			 * to forward samples from on path to another.
			 */
			bool isSecondary = n->sources.size() > 0;

			/* Create new path source */
			if (isSecondary) {
				/* Get master path source */
				auto mps = std::dynamic_pointer_cast<MasterPathSource>(n->sources.front());
				if (!mps)
					throw RuntimeError("Failed to find master path source");

				auto sps = std::make_shared<SecondaryPathSource>(this, n, nodes, mps);
				if (!sps)
					throw MemoryAllocationError();

				mps->addSecondary(sps);

				ps = sps;
			}
			else {
				ps = std::make_shared<MasterPathSource>(this, n);
				if (!ps)
					throw MemoryAllocationError();
			}

			if (masked.empty() || std::find(masked.begin(), masked.end(), n) != masked.end()) {
				ps->masked = true;
				mask.set(j);
			}

			/* Get the real node backing this path source
			 * In case of a secondary path source, its the internal loopback node!
			 */
			auto *rn = ps->getNode();

			rn->sources.push_back(ps);

			sources.push_back(ps);
			j++;
			psm[n] = ps;
		}

		SignalList::Ptr sigs = me->node->getInputSignals();

		/* Update signals of path */
		for (unsigned j = 0; j < (unsigned) me->length; j++) {
			Signal::Ptr sig;

			/* For data mappings we simple refer to the existing
			 * signal descriptors of the source node. */
			if (me->type == MappingEntry::Type::DATA) {
				sig = sigs->getByIndex(me->data.offset + j);
				if (!sig) {
					logger->warn("Failed to create signal description for path {}", *this);
					continue;
				}
			}
			/* For other mappings we create new signal descriptors */
			else {
				sig = me->toSignal(j);
				if (!sig)
					throw RuntimeError("Failed to create signal from mapping");
			}

			signals->resize(me->offset + j + 1);
			(*signals)[me->offset + j] = sig;
		}

		ps->mappings.push_back(me);
		i++;
	}

	/* Prepare path destinations */
	int mt_cnt = 0;
	for (auto pd : destinations) {
		auto *pd_mt = pd->node->getMemoryType();
		if (pd_mt != pool_mt) {
			if (mt_cnt > 0) {
				throw RuntimeError("Mixed memory types between path destinations");
			}

			pool_mt = pd_mt;
			mt_cnt++;
		}

		ret = pd->prepare(queuelen);
		if (ret)
			throw RuntimeError("Failed to prepare path destination {} of path {}", *pd->node, *this);
	}

	/* Autodetect whether to use original sequence numbers or not */
	if (original_sequence_no == -1)
		original_sequence_no = sources.size() == 1;

	/* Autodetect whether to use poll() for this path or not */
	if (poll == -1) {
		if (rate > 0)
			poll = 1;
		else if (sources.size()> 1)
			poll = 1;
		else
			poll = 0;
	}

#ifdef WITH_HOOKS
	/* Prepare path hooks */
	int m = builtin
		? (int) Hook::Flags::PATH |
		  (int) Hook::Flags::BUILTIN
		: 0;

	/* Add internal hooks if they are not already in the list */
	hooks.prepare(signals, m, this, nullptr);
	hooks.dump(logger, fmt::format("path {}", *this));
#endif /* WITH_HOOKS */

	/* Prepare pool */
	auto osigs = getOutputSignals();
	unsigned pool_size = MAX(1UL, destinations.size()) * queuelen;

	ret = pool_init(&pool, pool_size, SAMPLE_LENGTH(osigs->size()), pool_mt);
	if (ret)
		throw RuntimeError("Failed to initialize pool of path: {}", *this);

	logger->debug("Prepared path {} with {} output signals:", *this, osigs->size());
	if (logger->level() <= spdlog::level::debug)
		osigs->dump(logger);

	checkPrepared();

	state = State::PREPARED;
}

void Path::parse(json_t *json, NodeList &nodes, const uuid_t sn_uuid)
{
	int ret, en = -1, rev = -1;

	json_error_t err;
	json_t *json_in;
	json_t *json_out = nullptr;
	json_t *json_hooks = nullptr;
	json_t *json_mask = nullptr;

	const char *mode_str = nullptr;
	const char *uuid_str = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s: o, s?: o, s?: o, s?: b, s?: b, s?: b, s?: i, s?: s, s?: b, s?: F, s?: o, s?: b, s?: s, s?: i }",
		"in", &json_in,
		"out", &json_out,
		"hooks", &json_hooks,
		"reverse", &rev,
		"enabled", &en,
		"builtin", &builtin,
		"queuelen", &queuelen,
		"mode", &mode_str,
		"poll", &poll,
		"rate", &rate,
		"mask", &json_mask,
		"original_sequence_no", &original_sequence_no,
		"uuid", &uuid_str,
		"affinity", &affinity
	);
	if (ret)
		throw ConfigError(json, err, "node-config-path", "Failed to parse path configuration");

	if (en >= 0)
		enabled = en != 0;

	if (rev >= 0)
		reversed = rev != 0;

	/* Optional settings */
	if (mode_str) {
		if      (!strcmp(mode_str, "any"))
			mode = Mode::ANY;
		else if (!strcmp(mode_str, "all"))
			mode = Mode::ALL;
		else
			throw ConfigError(json, "node-config-path", "Invalid path mode '{}'", mode_str);
	}

	/* UUID */
	if (uuid_str) {
		ret = uuid_parse(uuid_str, uuid);
		if (ret)
			throw ConfigError(json, "node-config-path-uuid", "Failed to parse UUID: {}", uuid_str);
	}
	else
		/* Generate UUID from hashed config */
		uuid::generateFromJson(uuid, json, sn_uuid);

	/* Input node(s) */
	ret = mappings.parse(json_in);
	if (ret)
		throw ConfigError(json_in, "node-config-path-in", "Failed to parse input mapping of path {}", *this);

	/* Output node(s) */
	NodeList dests;
	if (json_out) {
		ret = dests.parse(json_out, nodes);
		if (ret)
			throw ConfigError(json_out, "node-config-path-out", "Failed to parse output nodes");
	}

	for (auto *n : dests) {
		if (n->out.path)
			throw ConfigError(json, "node-config-path", "Every node must only be used by a single path as destination");

		n->out.path = this;

		auto pd = std::make_shared<PathDestination>(this, n);
		if (!pd)
			throw MemoryAllocationError();

		n->destinations.push_back(pd);
		destinations.push_back(pd);
	}

#ifdef WITH_HOOKS
	if (json_hooks)
		hooks.parse(json_hooks, (int) Hook::Flags::PATH, this, nullptr);
#endif /* WITH_HOOKS */

	if (json_mask)
		parseMask(json_mask, nodes);

	config = json;
	state = State::PARSED;
}

void Path::parseMask(json_t *json_mask, NodeList &nodes)
{
	json_t *json_entry;
	size_t i;

	if (!json_is_array(json_mask))
		throw ConfigError(json_mask, "node-config-path-mask", "The 'mask' setting must be a list of node names");

	json_array_foreach(json_mask, i, json_entry) {
		const char *name;
		Node *node;

		name = json_string_value(json_entry);
		if (!name)
			throw ConfigError(json_mask, "node-config-path-mask", "The 'mask' setting must be a list of node names");

		node = nodes.lookup(name);
		if (!node)
			throw ConfigError(json_mask, "node-config-path-mask", "The 'mask' entry '{}' is not a valid node name", name);

		masked.push_back(node);
	}
}

void Path::check()
{
	assert(state != State::DESTROYED);

	if (rate < 0)
		throw RuntimeError("Setting 'rate' of path {} must be a positive number.", *this);

	if (!IS_POW2(queuelen)) {
		queuelen = LOG2_CEIL(queuelen);
		logger->warn("Queue length should always be a power of 2. Adjusting to {}", queuelen);
	}

#ifdef WITH_HOOKS
	hooks.check();
#endif /* WITH_HOOKS */

	state = State::CHECKED;
}

void Path::checkPrepared()
{
	if (poll == 0) {
		/* Check that we do not need to multiplex between multiple sources when polling is disabled */
		if (sources.size() > 1)
			throw RuntimeError("Setting 'poll' must be active if the path has more than one source");

		/* Check that we do not use the fixed rate feature when polling is disabled */
		if (rate > 0)
			throw RuntimeError("Setting 'poll' must be activated when used together with setting 'rate'");
	}
	else {
		if (rate <= 0) {
			/* Check that all path sources provide a file descriptor for polling if fixed rate is disabled */
			for (auto ps : sources) {
				if (!(ps->getNode()->getFactory()->getFlags() & (int) NodeFactory::Flags::SUPPORTS_POLL))
					throw RuntimeError("Node {} can not be used in polling mode with path {}", *ps->getNode(), *this);
			}
		}
	}
}

void Path::start()
{
	int ret;
	const char *mode_str;

	assert(state == State::PREPARED);

	switch (mode) {
		case Mode::ANY:
			mode_str = "any";
			break;

		case Mode::ALL:
			mode_str = "all";
			break;

		default:
			mode_str = "unknown";
			break;
	}

	logger->info("Starting path {}: #signals={}/{}, #hooks={}, #sources={}, "
	                "#destinations={}, mode={}, poll={}, mask=0b{:b}, rate={}, "
	                "enabled={}, reversed={}, queuelen={}, original_sequence_no={}",
		*this,
		signals->size(),
		getOutputSignals()->size(),
		hooks.size(),
		sources.size(),
		destinations.size(),
		mode_str,
		poll ? "yes" : "no",
		mask.to_ullong(),
		rate,
		isEnabled() ? "yes" : "no",
		isReversed() ? "yes" : "no",
		queuelen,
		original_sequence_no ? "yes" : "no"
	);

#ifdef WITH_HOOKS
	hooks.start();
#endif /* WITH_HOOKS */

	last_sequence = 0;

	received.reset();

	/* We initialize the intial sample */
	last_sample = sample_alloc(&pool);
	if (!last_sample)
		throw MemoryAllocationError();

	last_sample->length = signals->size();
	last_sample->signals = signals;

	last_sample->ts.origin = time_now();
	last_sample->flags = (int) SampleFlags::HAS_TS_ORIGIN;

	last_sample->sequence = 0;
	last_sample->flags |= (int) SampleFlags::HAS_SEQUENCE;

	if (last_sample->length > 0)
		last_sample->flags |= (int) SampleFlags::HAS_DATA;

	for (size_t i = 0; i < last_sample->length; i++) {
		auto sig = signals->getByIndex(i);

		last_sample->data[i] = sig->init;
	}

	if (poll > 0)
		startPoll();

	state = State::STARTED;

	/* Start one thread per path for sending to destinations
	 *
	 * Special case: If the path only has a single source and this source
	 * does not offer a file descriptor for polling, we will use a special
	 * thread function.
	 */
	ret = pthread_create(&tid, nullptr, runWrapper, this);
	if (ret)
		throw RuntimeError("Failed to create path thread");

	if (affinity)
		kernel::rt::setThreadAffinity(tid, affinity);
}

void Path::stop()
{
	int ret;

	if (state != State::STARTED &&
	    state != State::STOPPING)
		return;

	logger->info("Stopping path: {}", *this);

	if (state != State::STOPPING)
		state = State::STOPPING;

	/* Cancel the thread in case is currently in a blocking syscall.
	 *
	 * We dont care if the thread has already been terminated.
	 */
	ret = pthread_cancel(tid);
	if (ret && ret != ESRCH)
	 	throw RuntimeError("Failed to cancel path thread");

	ret = pthread_join(tid, nullptr);
	if (ret)
		throw RuntimeError("Failed to join path thread");

#ifdef WITH_HOOKS
	hooks.stop();
#endif /* WITH_HOOKS */

	sample_decref(last_sample);

	state = State::STOPPED;
}

Path::~Path()
{
	int ret __attribute__((unused));

	assert(state != State::DESTROYED);

	ret = pool_destroy(&pool);
}

bool Path::isSimple() const
{
	int ret;
	const char *in = nullptr, *out = nullptr;

	json_error_t err;
	ret = json_unpack_ex(config, &err, 0, "{ s: s, s: s }", "in", &in, "out", &out);
	if (ret)
		return false;

	ret = Node::isValidName(in);
	if (!ret)
		return false;

	ret = Node::isValidName(out);
	if (!ret)
		return false;

	return true;
}

bool Path::isMuxed() const
{
	if (sources.size() > 0)
		return true;

	if (mappings.size() > 0)
		return true;

	auto me = mappings.front();

	if (me->type != MappingEntry::Type::DATA)
		return true;

	if (me->data.offset != 0)
		return true;

	if (me->length != -1)
		return true;

	return false;
}

SignalList::Ptr Path::getOutputSignals(bool after_hooks)
{
#ifdef WITH_HOOKS
	if (after_hooks && hooks.size() > 0)
		return hooks.getSignals();
#endif /* WITH_HOOKS */

	return signals;
}

unsigned Path::getOutputSignalsMaxCount()
{
#ifdef WITH_HOOKS
	if (hooks.size() > 0)
		return MAX(signals->size(), hooks.getSignalsMaxCount());
#endif /* WITH_HOOKS */

	return signals->size();
}

json_t * Path::toJson() const
{
	char uuid_str[37];
	uuid_unparse(uuid, uuid_str);

	json_t *json_signals = signals->toJson();
#ifdef WITH_HOOKS
	json_t *json_hooks = hooks.toJson();
#else
	json_t *json_hooks = json_array();
#endif /* WITH_HOOKS */
	json_t *json_sources = json_array();
	json_t *json_destinations = json_array();

	for (auto ps : sources)
		json_array_append_new(json_sources, json_string(ps->node->getNameShort().c_str()));

	for (auto pd : destinations)
		json_array_append_new(json_destinations, json_string(pd->node->getNameShort().c_str()));

	json_t *json_path = json_pack("{ s: s, s: s, s: s, s: b, s: b s: b, s: b, s: b, s: b s: i, s: o, s: o, s: o, s: o }",
		"uuid", uuid_str,
		"state", stateToString(state).c_str(),
		"mode", mode == Mode::ANY ? "any" : "all",
		"enabled", enabled,
		"builtin", builtin,
		"reversed", reversed,
		"original_sequence_no", original_sequence_no,
		"last_sequence", last_sequence,
		"poll", poll,
		"queuelen", queuelen,
		"signals", json_signals,
		"hooks", json_hooks,
		"in", json_sources,
		"out", json_destinations
	);

	return json_path;
}

int villas::node::Path::id = 0;
