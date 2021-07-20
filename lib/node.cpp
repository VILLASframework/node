/** Nodes.
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

#include <regex>
#include <cstring>
#include <cctype>
#include <openssl/md5.h>

#include <villas/node/config.h>
#include <villas/hook.hpp>
#include <villas/hook_list.hpp>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/node_list.hpp>
#include <villas/path.h>
#include <villas/utils.hpp>
#include <villas/colors.hpp>
#include <villas/mapping.h>
#include <villas/timing.h>
#include <villas/signal.h>
#include <villas/memory.h>

#ifdef WITH_NETEM
  #include <villas/kernel/if.hpp>
  #include <villas/kernel/nl.hpp>
  #include <villas/kernel/tc.hpp>
  #include <villas/kernel/tc_netem.hpp>
#endif /* WITH_NETEM */

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int node_init(struct vnode *n, struct vnode_type *vt)
{
	int ret;

	n->_vt = vt;
	n->_vd = new char[vt->size];
	if (!n->_vd)
		throw MemoryAllocationError();

	memset(n->_vd, 0, vt->size);

	using stats_ptr = std::shared_ptr<Stats>;

	new (&n->stats) stats_ptr();
	new (&n->logger) Logger();

	n->logger = logging.get(fmt::format("node:{}", *vt));

	uuid_clear(n->uuid);

	n->name = nullptr;
	n->_name = nullptr;
	n->_name_long = nullptr;
	n->enabled = true;
	n->affinity = -1; /* all cores */

#ifdef __linux__
	n->fwmark = -1;
#endif /* __linux__ */

#ifdef WITH_NETEM
	n->tc_qdisc = nullptr;
	n->tc_classifier = nullptr;
#endif /* WITH_NETEM */

	/* Default values */
	ret = node_direction_init(&n->in, NodeDir::IN, n);
	if (ret)
		return ret;

	ret = node_direction_init(&n->out, NodeDir::OUT, n);
	if (ret)
		return ret;

	ret = vlist_init(&n->sources);
	if (ret)
		return ret;

	ret = vlist_init(&n->destinations);
	if (ret)
		return ret;

	ret = vt->init ? vt->init(n) : 0;
	if (ret)
		return ret;

	n->state = State::INITIALIZED;

	vt->instances.push_back(n);

	return 0;
}

int node_prepare(struct vnode *n)
{
	int ret;

	assert(n->state == State::CHECKED);

	ret = node_type(n)->prepare ? node_type(n)->prepare(n) : 0;
	if (ret)
		return ret;

	ret = node_direction_prepare(&n->in, n);
	if (ret)
		return ret;

	ret = node_direction_prepare(&n->out, n);
	if (ret)
		return ret;

	n->state = State::PREPARED;

	return 0;
}

int node_parse(struct vnode *n, json_t *json, const uuid_t sn_uuid)
{
	int ret, enabled = n->enabled;

	json_error_t err;
	json_t *json_netem = nullptr;

	const char *uuid_str = nullptr;
	const char *name_str = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: s, s?: s, s?: b }",
		"name", &name_str,
		"uuid", &uuid_str,
		"enabled", &enabled
	);
	if (ret)
		return ret;

	if (name_str)
		n->logger = logging.get(fmt::format("node:{}", name_str));

#ifdef __linux__
	ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: o, s?: i } }",
		"out",
			"netem", &json_netem,
			"fwmark", &n->fwmark
	);
	if (ret)
		return ret;
#endif /* __linux__ */

	n->enabled = enabled;

	if (name_str)
		n->name = strdup(name_str);
	else
		n->name = strdup("<none>");

	if (uuid_str) {
		ret = uuid_parse(uuid_str, n->uuid);
		if (ret)
			throw ConfigError(json, "node-config-node-uuid", "Failed to parse UUID: {}", uuid_str);
	}
	else
		/* Generate UUID from hashed config including node name */
		uuid_generate_from_json(n->uuid, json, sn_uuid);

	if (json_netem) {
#ifdef WITH_NETEM
		int enabled = 1;

		ret = json_unpack_ex(json_netem, &err, 0, "{ s?: b }",  "enabled", &enabled);
		if (ret)
			return ret;

		if (enabled)
			kernel::tc::netem_parse(&n->tc_qdisc, json_netem);
		else
			n->tc_qdisc = nullptr;
#endif /* WITH_NETEM */
	}

	struct {
		const char *str;
		struct vnode_direction *dir;
	} dirs[] = {
		{ "in", &n->in },
		{ "out", &n->out }
	};

	const char *fields[] = { "signals", "builtin", "vectorize", "hooks" };

	for (unsigned j = 0; j < ARRAY_LEN(dirs); j++) {
		json_t *json_dir = json_object_get(json, dirs[j].str);

		/* Skip if direction is unused */
		if (!json_dir)
			json_dir = json_pack("{ s: b }", "enabled", 0);

		/* Copy missing fields from main node config to direction config */
		for (unsigned i = 0; i < ARRAY_LEN(fields); i++) {
			json_t *json_field_dir  = json_object_get(json_dir, fields[i]);
			json_t *json_field_node = json_object_get(json, fields[i]);

			if (json_field_node && !json_field_dir)
				json_object_set(json_dir, fields[i], json_field_node);
		}

		ret = node_direction_parse(dirs[j].dir, n, json_dir);
		if (ret)
			return ret;
	}

	ret = node_type(n)->parse ? node_type(n)->parse(n, json) : 0;
	if (ret)
		return ret;

	n->config = json;
	n->state = State::PARSED;

	return 0;
}

int node_check(struct vnode *n)
{
	int ret;
	assert(n->state != State::DESTROYED);

	node_direction_check(&n->in, n);
	node_direction_check(&n->out, n);

	ret = node_type(n)->check ? node_type(n)->check(n) : 0;
	if (ret)
		return ret;

	n->state = State::CHECKED;

	return 0;
}

int node_start(struct vnode *n)
{
	int ret;

	assert(n->state == State::PREPARED);
	assert(node_type(n)->state == State::STARTED);

	n->logger->info("Starting node {}", node_name_long(n));

	ret = node_direction_start(&n->in, n);
	if (ret)
		return ret;

	ret = node_direction_start(&n->out, n);
	if (ret)
		return ret;

	ret = node_type(n)->start ? node_type(n)->start(n) : 0;
	if (ret)
		return ret;

#ifdef __linux__
	/* Set fwmark for outgoing packets if netem is enabled for this node */
	if (n->fwmark >= 0) {
		int fds[16];
		int num_sds = node_netem_fds(n, fds);

		for (int i = 0; i < num_sds; i++) {
			int fd = fds[i];

			ret = setsockopt(fd, SOL_SOCKET, SO_MARK, &n->fwmark, sizeof(n->fwmark));
			if (ret)
				throw RuntimeError("Failed to set FW mark for outgoing packets");
			else
				n->logger->debug("Set FW mark for socket (sd={}) to {}", fd, n->fwmark);
		}
	}
#endif /* __linux__ */

	n->state = State::STARTED;
	n->sequence = 0;

	return 0;
}

int node_stop(struct vnode *n)
{
	int ret;

	if (n->state != State::STOPPING && n->state != State::STARTED && n->state != State::CONNECTED && n->state != State::PENDING_CONNECT)
		return 0;

	n->logger->info("Stopping node");

	ret = node_direction_stop(&n->in, n);
	if (ret)
		return ret;

	ret = node_direction_stop(&n->out, n);
	if (ret)
		return ret;

	ret = node_type(n)->stop ? node_type(n)->stop(n) : 0;

	if (ret == 0)
		n->state = State::STOPPED;

	return ret;
}

int node_pause(struct vnode *n)
{
	int ret;

	if (n->state != State::STARTED)
		return -1;

	n->logger->info("Pausing node");

	ret = node_type(n)->pause ? node_type(n)->pause(n) : 0;

	if (ret == 0)
		n->state = State::PAUSED;

	return ret;
}

int node_resume(struct vnode *n)
{
	int ret;

	if (n->state != State::PAUSED)
		return -1;

	n->logger->info("Resuming node");

	ret = node_type(n)->resume ? node_type(n)->resume(n) : 0;

	if (ret == 0)
		n->state = State::STARTED;

	return ret;
}

int node_restart(struct vnode *n)
{
	int ret;

	if (n->state != State::STARTED)
		return -1;

	n->logger->info("Restarting node");

	if (node_type(n)->restart)
		ret = node_type(n)->restart(n);
	else {
		ret = node_type(n)->stop ? node_type(n)->stop(n) : 0;
		if (ret)
			return ret;

		ret = node_type(n)->start ? node_type(n)->start(n) : 0;
		if (ret)
			return ret;
	}

	return 0;
}

int node_destroy(struct vnode *n)
{
	int ret;
	assert(n->state != State::DESTROYED && n->state != State::STARTED);

	ret = node_direction_destroy(&n->in, n);
	if (ret)
		return ret;

	ret = node_direction_destroy(&n->out, n);
	if (ret)
		return ret;

	ret = vlist_destroy(&n->sources);
	if (ret)
		return ret;

	ret = vlist_destroy(&n->destinations);
	if (ret)
		return ret;

	if (node_type(n)->destroy) {
		ret = (int) node_type(n)->destroy(n);
		if (ret)
			return ret;
	}

	node_type(n)->instances.remove(n);

	if (n->_vd)
		delete[] (char *) n->_vd;

	if (n->_name)
		free(n->_name);

	if (n->_name_long)
		free(n->_name_long);

	if (n->name)
		free(n->name);

#ifdef WITH_NETEM
	rtnl_qdisc_put(n->tc_qdisc);
	rtnl_cls_put(n->tc_classifier);
#endif /* WITH_NETEM */

	using stats_ptr = std::shared_ptr<Stats>;

	n->stats.~stats_ptr();

	n->state = State::DESTROYED;

	return 0;
}

int node_read(struct vnode *n, struct sample * smps[], unsigned cnt)
{
	int toread, readd, nread = 0;
	unsigned vect;

	assert(node_type(n)->read);

	if (n->state == State::PAUSED || n->state == State::PENDING_CONNECT)
		return 0;
	else if (n->state != State::STARTED && n->state != State::CONNECTED)
		return -1;

	vect = node_type(n)->vectorize;
	if (!vect)
		vect = cnt;

	while (cnt - nread > 0) {
		toread = MIN(cnt - nread, vect);
		readd = node_type(n)->read(n, &smps[nread], toread);
		if (readd < 0)
			return readd;

		nread += readd;
	}


#ifdef WITH_HOOKS
	/* Run read hooks */
	int rread = hook_list_process(&n->in.hooks, smps, nread);
	if (rread < 0)
		return rread;

	int skipped = nread - rread;
	if (skipped > 0) {
		if (n->stats != nullptr)
			n->stats->update(Stats::Metric::SMPS_SKIPPED, skipped);

		n->logger->debug("Received {} samples of which {} have been skipped", nread, skipped);
	}
	else
		n->logger->debug( "Received {} samples", nread);

	return rread;
#else
	n->logger->debug("Received {} samples", nread);

	return nread;
#endif /* WITH_HOOKS */
}

int node_write(struct vnode *n, struct sample * smps[], unsigned cnt)
{
	int tosend, sent, nsent = 0;
	unsigned vect;

	assert(node_type(n)->write);

	if (n->state == State::PAUSED || n->state == State::PENDING_CONNECT)
		return 0;
	else if (n->state != State::STARTED && n->state != State::CONNECTED)
		return -1;

#ifdef WITH_HOOKS
	/* Run write hooks */
	cnt = hook_list_process(&n->out.hooks, smps, cnt);
	if (cnt <= 0)
		return cnt;
#endif /* WITH_HOOKS */

	vect = node_type(n)->vectorize;
	if (!vect)
		vect = cnt;

	while (cnt - nsent > 0) {
		tosend = MIN(cnt - nsent, vect);
		sent = node_type(n)->write(n, &smps[nsent], tosend);
		if (sent < 0)
			return sent;

		nsent += sent;
		n->logger->debug("Sent {} samples", sent);
	}

	return nsent;
}

char * node_name(struct vnode *n)
{
	if (!n->_name)
		strcatf(&n->_name, CLR_RED("%s") "(" CLR_YEL("%s") ")", n->name, node_type_name(node_type(n)));

	return n->_name;
}

char * node_name_long(struct vnode *n)
{
	if (!n->_name_long) {
		char uuid[37];
		uuid_unparse(n->uuid, uuid);

		strcatf(&n->_name_long, "%s: uuid=%s, #in.signals=%zu(%zu), #out.signals=%zu(%zu), #in.hooks=%zu, #out.hooks=%zu, in.vectorize=%d, out.vectorize=%d",
			node_name(n), uuid,
			vlist_length(&n->in.signals),
			node_input_signals(n) ? vlist_length(node_input_signals(n)) : 0,
			vlist_length(&n->out.signals),
			node_output_signals(n) ? vlist_length(node_output_signals(n)) : 0,
			vlist_length(&n->in.hooks),    vlist_length(&n->out.hooks),
			n->in.vectorize, n->out.vectorize
		);

#ifdef WITH_NETEM
		strcatf(&n->_name_long, ", out.netem=%s", n->tc_qdisc ? "yes" : "no");

		if (n->tc_qdisc)
			strcatf(&n->_name_long, ", fwmark=%d", n->fwmark);
#endif /* WITH_NETEM */

		if (n->out.path) {
			auto pn = fmt::format("{}", *n->out.path);
			strcatf(&n->_name_long, ", out.path=%s", pn.c_str());
		}

		if (node_type(n)->print) {
			struct vnode_type *vt = node_type(n);

			/* Append node-type specific details */
			char *name_long = vt->print(n);
			strcatf(&n->_name_long, ", %s", name_long);
			free(name_long);
		}
	}

	return n->_name_long;
}

const char * node_name_short(struct vnode *n)
{
	return n->name;
}

int node_reverse(struct vnode *n)
{
	return node_type(n)->reverse ? node_type(n)->reverse(n) : -1;
}

int node_poll_fds(struct vnode *n, int fds[])
{
	return node_type(n)->poll_fds ? node_type(n)->poll_fds(n, fds) : -1;
}

int node_netem_fds(struct vnode *n, int fds[])
{
	return node_type(n)->netem_fds ? node_type(n)->netem_fds(n, fds) : -1;
}

struct memory_type * node_memory_type(struct vnode *n)
{
	return node_type(n)->memory_type ? node_type(n)->memory_type(n, memory_default) : memory_default;
}

int node_list_parse(struct vlist *list, json_t *json, NodeList &all)
{
	struct vnode *node;
	const char *str;
	char *allstr = nullptr;

	size_t index;
	json_t *elm;

	auto logger = logging.get("node");

	switch (json_typeof(json)) {
		case JSON_STRING:
			str = json_string_value(json);
			node = all.lookup(str);
			if (!node)
				goto invalid2;

			vlist_push(list, node);
			break;

		case JSON_ARRAY:
			json_array_foreach(json, index, elm) {
				if (!json_is_string(elm))
					goto invalid;

				str = json_string_value(elm);
				node = all.lookup(str);
				if (!node)
					goto invalid;

				vlist_push(list, node);
			}
			break;

		default:
			goto invalid;
	}

	return 0;

invalid:
	throw RuntimeError("The node list must be an a single or an array of strings referring to the keys of the 'nodes' section");

	return -1;

invalid2:
	for (auto *n : all)
		strcatf(&allstr, " %s", node_name_short(n));

	throw RuntimeError("Unknown node {}. Choose of one of: {}", str, allstr);

	return 0;
}

bool node_is_valid_name(const char *name)
{
	std::regex re(RE_NODE_NAME);

	return std::regex_match(name, re);
}

bool node_is_enabled(const struct vnode *n)
{
	return n->enabled;
}

struct vlist * node_input_signals(struct vnode *n)
{
	return node_direction_get_signals(&n->in);
}

struct vlist * node_output_signals(struct vnode *n)
{
	if (n->out.path)
		return path_output_signals(n->out.path);

	return nullptr;
}

unsigned node_input_signals_max_cnt(struct vnode *n)
{
	return node_direction_get_signals_max_cnt(&n->in);
}

unsigned node_output_signals_max_cnt(struct vnode *n)
{
	if (n->out.path)
		return path_output_signals_max_cnt(n->out.path);

	return 0;
}

json_t * node_to_json(struct vnode *n)
{
	struct vlist *output_signals;

	json_t *json_node;
	json_t *json_signals_in = nullptr;
	json_t *json_signals_out = nullptr;

	char uuid[37];
	uuid_unparse(n->uuid, uuid);

	json_signals_in = signal_list_to_json(&n->in.signals);

	output_signals = node_output_signals(n);
	if (output_signals)
		json_signals_out = signal_list_to_json(output_signals);

	json_node = json_pack("{ s: s, s: s, s: s, s: i, s: { s: i, s: o? }, s: { s: i, s: o? } }",
		"name",		node_name_short(n),
		"uuid", 	uuid,
		"state",	state_print(n->state),
		"affinity",	n->affinity,
		"in",
			"vectorize",	n->in.vectorize,
			"signals", 	json_signals_in,
		"out",
			"vectorize",	n->out.vectorize,
			"signals",	json_signals_out
	);

	if (n->stats)
		json_object_set_new(json_node, "stats", n->stats->toJson());

	/* Add all additional fields of node here.
	 * This can be used for metadata */
	json_object_update(json_node, n->config);

	return json_node;
}
