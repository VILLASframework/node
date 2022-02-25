/** Nodes.
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

#include <regex>
#include <cstring>
#include <cctype>
#include <openssl/md5.h>

#ifdef __linux__
  #include <sys/types.h>
  #include <sys/socket.h>
#endif

#include <villas/node/config.hpp>
#include <villas/hook.hpp>
#include <villas/hook_list.hpp>
#include <villas/sample.hpp>
#include <villas/node.hpp>
#include <villas/node_list.hpp>
#include <villas/path.hpp>
#include <villas/utils.hpp>
#include <villas/uuid.hpp>
#include <villas/colors.hpp>
#include <villas/mapping.hpp>
#include <villas/timing.hpp>
#include <villas/signal.hpp>
#include <villas/node/memory.hpp>

#ifdef WITH_NETEM
  #include <villas/kernel/if.hpp>
  #include <villas/kernel/nl.hpp>
  #include <villas/kernel/tc.hpp>
  #include <villas/kernel/tc_netem.hpp>
#endif /* WITH_NETEM */

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

Node::Node(const std::string &n) :
	logger(logging.get("node")),
	sequence_init(0),
	sequence(0),
	in(NodeDirection::Direction::IN, this),
	out(NodeDirection::Direction::OUT, this),
#ifdef __linux__
	fwmark(-1),
#endif /* __linux__ */
#ifdef WITH_NETEM
	tc_qdisc(nullptr),
	tc_classifier(nullptr),
#endif /* WITH_NETEM */
	state(State::INITIALIZED),
	enabled(true),
	config(nullptr),
	name_short(n),
	name_long(),
	affinity(-1), /* all cores */
	factory(nullptr)
{
	uuid_clear(uuid);

	name_long = fmt::format(CLR_RED("{}"), n);
}

Node::~Node()
{
#ifdef WITH_NETEM
	rtnl_qdisc_put(tc_qdisc);
	rtnl_cls_put(tc_classifier);
#endif /* WITH_NETEM */

	factory->instances.remove(this);
}

int Node::prepare()
{
	int ret;

	ret = in.prepare();
	if (ret)
		return ret;

	ret = out.prepare();
	if (ret)
		return ret;

	state = State::PREPARED;

	return 0;
}

int Node::parse(json_t *json, const uuid_t sn_uuid)
{
	assert(state == State::INITIALIZED ||
	       state == State::PARSED ||
	       state == State::CHECKED);

	int ret, en = enabled, init_seq = -1;

	json_error_t err;
	json_t *json_netem = nullptr;

	const char *uuid_str = nullptr;
	const char *name_str = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: b, s?: i }",
		"name", &name_str,
		"uuid", &uuid_str,
		"enabled", &en,
		"initial_sequenceno", &init_seq
	);
	if (ret)
		return ret;

	if (init_seq >= 0)
		sequence_init = init_seq;

	if (name_str)
		logger = logging.get(fmt::format("node:{}", name_str));

#ifdef __linux__
	ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: o, s?: i } }",
		"out",
			"netem", &json_netem,
			"fwmark", &fwmark
	);
	if (ret)
		return ret;
#endif /* __linux__ */

	enabled = en;

	if (name_str) {
		name_short = std::string(name_str);
		name_long = fmt::format(CLR_RED("{}") "(" CLR_YEL("{}") ")", name_str, factory->getName());
	}
	else if (name_short.empty()) {
		name_short = "<none>";
		name_long = CLR_RED("<none>");
	}

	if (uuid_str) {
		ret = uuid_parse(uuid_str, uuid);
		if (ret)
			throw ConfigError(json, "node-config-node-uuid", "Failed to parse UUID: {}", uuid_str);
	}
	else
		/* Generate UUID from hashed config including node name */
		uuid::generateFromJson(uuid, json, sn_uuid);

	if (json_netem) {
#ifdef WITH_NETEM
		int enabled = 1;

		ret = json_unpack_ex(json_netem, &err, 0, "{ s?: b }",  "enabled", &enabled);
		if (ret)
			return ret;

		if (enabled)
			kernel::tc::netem_parse(&tc_qdisc, json_netem);
		else
			tc_qdisc = nullptr;
#endif /* WITH_NETEM */
	}

	struct {
		const char *str;
		struct NodeDirection *dir;
	} dirs[] = {
		{ "in", &in },
		{ "out", &out }
	};

	const char *fields[] = { "signals", "builtin", "vectorize", "hooks" };

	for (unsigned j = 0; j < ARRAY_LEN(dirs); j++) {
		json_t *json_dir = json_object_get(json, dirs[j].str);

		/* Skip if direction is unused */
		if (!json_dir) {
			json_dir = json_pack("{ s: b }", "enabled", 0);
			// json_object_set_new(json, dirs[j].str, json_dir);
		}

		/* Copy missing fields from main node config to direction config */
		for (unsigned i = 0; i < ARRAY_LEN(fields); i++) {
			json_t *json_field_dir  = json_object_get(json_dir, fields[i]);
			json_t *json_field_node = json_object_get(json, fields[i]);

			if (json_field_node && !json_field_dir)
				json_object_set(json_dir, fields[i], json_field_node);
		}

		ret = dirs[j].dir->parse(json_dir);
		if (ret)
			return ret;
	}

	config = json;

	return 0;
}

int Node::check()
{
	assert(state == State::CHECKED ||
	       state == State::PARSED ||
	       state == State::INITIALIZED);

	in.check();
	out.check();

	state = State::CHECKED;

	return 0;
}

int Node::start()
{
	int ret;

	assert(state == State::PREPARED);

	logger->info("Starting node {}", getNameFull());

	ret = in.start();
	if (ret)
		return ret;

	ret = out.start();
	if (ret)
		return ret;

#ifdef __linux__
	/* Set fwmark for outgoing packets if netem is enabled for this node */
	if (fwmark >= 0) {
		for (int fd : getNetemFDs()) {
			ret = setsockopt(fd, SOL_SOCKET, SO_MARK, &fwmark, sizeof(fwmark));
			if (ret)
				throw RuntimeError("Failed to set FW mark for outgoing packets");
			else
				logger->debug("Set FW mark for socket (sd={}) to {}", fd, fwmark);
		}
	}
#endif /* __linux__ */

	state = State::STARTED;
	sequence = sequence_init;

	return 0;
}

int Node::stop()
{
	int ret;

	if (state != State::STOPPING && state != State::STARTED && state != State::CONNECTED && state != State::PENDING_CONNECT)
		return 0;

	logger->info("Stopping node");

	ret = in.stop();
	if (ret)
		return ret;

	ret = out.stop();
	if (ret)
		return ret;

	return 0;
}

int Node::restart()
{
	int ret;

	assert(state == State::STARTED);

	logger->info("Restarting node");

	ret = stop();
	if (ret)
		return ret;

	ret = start();
	if (ret)
		return ret;

	return 0;
}

int Node::read(struct Sample * smps[], unsigned cnt)
{
	int toread, readd, nread = 0;
	unsigned vect;

	if (state == State::PAUSED || state == State::PENDING_CONNECT)
		return 0;
	else if (state != State::STARTED && state != State::CONNECTED)
		return -1;

	vect = factory->getVectorize();
	if (!vect)
		vect = cnt;

	while (cnt - nread > 0) {
		toread = MIN(cnt - nread, vect);
		readd = _read(&smps[nread], toread);
		if (readd < 0)
			return readd;

		nread += readd;
	}

#ifdef WITH_HOOKS
	/* Run read hooks */
	int rread = in.hooks.process(smps, nread);
	if (rread < 0)
		return rread;

	int skipped = nread - rread;
	if (skipped > 0) {
		if (stats != nullptr)
			stats->update(Stats::Metric::SMPS_SKIPPED, skipped);

		logger->debug("Received {} samples of which {} have been skipped", nread, skipped);
	}
	else
		logger->debug( "Received {} samples", nread);

	return rread;
#else
	logger->debug("Received {} samples", nread);

	return nread;
#endif /* WITH_HOOKS */
}

int Node::write(struct Sample * smps[], unsigned cnt)
{
	int tosend, sent, nsent = 0;
	unsigned vect;

	if (state == State::PAUSED || state == State::PENDING_CONNECT)
		return 0;
	else if (state != State::STARTED && state != State::CONNECTED)
		return -1;

#ifdef WITH_HOOKS
	/* Run write hooks */
	cnt = out.hooks.process(smps, cnt);
	if (cnt <= 0)
		return cnt;
#endif /* WITH_HOOKS */

	vect = getFactory()->getVectorize();
	if (!vect)
		vect = cnt;

	while (cnt - nsent > 0) {
		tosend = MIN(cnt - nsent, vect);
		sent = _write(&smps[nsent], tosend);
		if (sent < 0)
			return sent;

		nsent += sent;
		logger->debug("Sent {} samples", sent);
	}

	return nsent;
}

const std::string & Node::getNameFull()
{
	if (name_full.empty()) {
		char uuid_str[37];
		uuid_unparse(uuid, uuid_str);

		name_full = fmt::format("{}: uuid={}, #in.signals={}/{}, #in.hooks={}, #out.hooks={}, in.vectorize={}, out.vectorize={}",
			getName(), uuid_str,
			getInputSignals(false)->size(),
			getInputSignals(true)->size(),
			in.hooks.size(), out.hooks.size(),
			in.vectorize, out.vectorize
		);

#ifdef WITH_NETEM
		name_full += fmt::format(", out.netem={}", tc_qdisc ? "yes" : "no");

		if (tc_qdisc)
			name_full += fmt::format(", fwmark={}", fwmark);
#endif /* WITH_NETEM */

		if (out.path) {
			name_full += fmt::format(", #out.signals={}/{}",
				getOutputSignals(false) ? getOutputSignals(false)->size() : 0,
				getOutputSignals()      ? getOutputSignals()->size()      : 0);

			name_full += fmt::format(", out.path={}", *out.path);
		}

		/* Append node-type specific details */
		auto details = getDetails();
		if (!details.empty())
			name_full += fmt::format(", {}", details);
	}

	return name_full;
}

SignalList::Ptr Node::getInputSignals(bool after_hooks) const
{
	return in.getSignals(after_hooks);
}

SignalList::Ptr Node::getOutputSignals(bool after_hooks) const
{
	if (out.path)
		return out.path->getOutputSignals();

	return nullptr;
}

unsigned Node::getInputSignalsMaxCount() const
{
	return in.getSignalsMaxCount();
}

unsigned Node::getOutputSignalsMaxCount() const
{
	if (out.path)
		return out.path->getOutputSignalsMaxCount();

	return 0;
}

bool Node::isValidName(const std::string &name)
{
	std::regex re(RE_NODE_NAME);

	return std::regex_match(name, re);
}

json_t * Node::toJson() const
{
	json_t *json_node;
	json_t *json_signals_in = nullptr;
	json_t *json_signals_out = nullptr;

	char uuid_str[37];
	uuid_unparse(uuid, uuid_str);

	json_signals_in = getInputSignals()->toJson();

	auto output_signals = getOutputSignals();
	if (output_signals)
		json_signals_out = output_signals->toJson();

	json_node = json_pack("{ s: s, s: s, s: s, s: i, s: { s: i, s: o? }, s: { s: i, s: o? } }",
		"name",		getNameShort().c_str(),
		"uuid", 	uuid_str,
		"state",	stateToString(state).c_str(),
		"affinity",	affinity,
		"in",
			"vectorize",	in.vectorize,
			"signals", 	json_signals_in,
		"out",
			"vectorize",	out.vectorize,
			"signals",	json_signals_out
	);

	if (stats)
		json_object_set_new(json_node, "stats", stats->toJson());

	/* Add all additional fields of node here.
	 * This can be used for metadata */
	json_object_update(json_node, config);

	return json_node;
}

void Node::swapSignals() {
	SWAP(in.signals, out.signals);
}

Node * NodeFactory::make(json_t *json, uuid_t uuid)
{
	int ret;
	std::string type;
	Node *n;

	if (json_is_string(json)) {
		type = json_string_value(json);

		return NodeFactory::make(type);
	}
	else if (json_is_object(json)) {
		json_t *json_type = json_object_get(json, "type");

		type = json_string_value(json_type);

		n = NodeFactory::make(type);
		if (!n)
			return nullptr;

		ret = n->parse(json, uuid);
		if (ret) {
			delete n;
			return nullptr;
		}

		return n;
	}
	else
		throw ConfigError(json, "node-config-node", "Invalid node config");
}

Node * NodeFactory::make(const std::string &type)
{
	NodeFactory *nf = plugin::registry->lookup<NodeFactory>(type);
	if (!nf)
		throw RuntimeError("Unknown node-type: {}", type);

	return nf->make();
}

int NodeFactory::start(SuperNode *sn)
{
	getLogger()->info("Initialized node type which is used by {} nodes", instances.size());

	state = State::STARTED;

	return 0;
}

int NodeFactory::stop()
{
	getLogger()->info("De-initialized node type");

	state = State::STOPPED;

	return 0;
}
