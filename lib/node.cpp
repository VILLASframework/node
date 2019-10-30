/** Nodes.
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

#include <cstring>
#include <cctype>
#include <openssl/md5.h>

#include <villas/node/config.h>
#include <villas/hook.hpp>
#include <villas/hook_list.hpp>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/utils.hpp>
#include <villas/colors.hpp>
#include <villas/plugin.h>
#include <villas/mapping.h>
#include <villas/timing.h>
#include <villas/signal.h>
#include <villas/memory.h>

#ifdef WITH_NETEM
  #include <villas/kernel/if.h>
  #include <villas/kernel/nl.h>
  #include <villas/kernel/tc.h>
  #include <villas/kernel/tc_netem.h>
#endif /* WITH_NETEM */

using namespace villas;
using namespace villas::utils;

int node_init(struct node *n, struct node_type *vt)
{
	int ret;
	assert(n->state == State::DESTROYED);

	n->_vt = vt;
	n->_vd = alloc(vt->size);

	//n->stats = nullptr;
	n->name = nullptr;
	n->_name = nullptr;
	n->_name_long = nullptr;
	n->enabled = 1;

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

	ret = vt->init ? vt->init(n) : 0;
	if (ret)
		return ret;

	n->state = State::INITIALIZED;

	vlist_push(&vt->instances, n);

	return 0;
}

int node_prepare(struct node *n)
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

int node_parse(struct node *n, json_t *json, const char *name)
{
	struct node_type *nt;
	int ret;

	json_error_t err;
	json_t *json_netem = nullptr;

	const char *type, *uuid = nullptr;

	n->name = strdup(name);

	ret = json_unpack_ex(json, &err, 0, "{ s: s, s?: s, s?: b }",
		"type", &type,
		"uuid", &uuid,
		"enabled", &n->enabled
	);
	if (ret)
		return ret;

#ifdef __linux__
	ret = json_unpack_ex(json, &err, 0, "{ s?: { s?: o, s?: i } }",
		"out",
			"netem", &json_netem,
			"fwmark", &n->fwmark
	);
	if (ret)
		return ret;
#endif /* __linux__ */

	nt = node_type_lookup(type);
	assert(nt == node_type(n));

	n->_vt = nt;

	if (uuid) {
		ret = uuid_parse(uuid, n->uuid);
		if (ret)
			throw ConfigError(json, "node-config-node-uuid", "Failed to parse UUID: {}", uuid);
	}
	else {
		/* Generate UUID from hashed config */
		char *json_str = json_dumps(json, JSON_COMPACT | JSON_SORT_KEYS);

		MD5((unsigned char*) json_str, strlen(json_str), (unsigned char*) &n->uuid);

		free(json_str);
	}

	if (json_netem) {
#ifdef WITH_NETEM
		int enabled = 1;

		ret = json_unpack_ex(json_netem, &err, 0, "{ s?: b }",  "enabled", &enabled);
		if (ret)
			return ret;

		if (enabled)
			tc_netem_parse(&n->tc_qdisc, json_netem);
		else
			n->tc_qdisc = nullptr;
#endif /* WITH_NETEM */
	}

	struct {
		const char *str;
		struct node_direction *dir;
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

	n->cfg = json;
	n->state = State::PARSED;

	return 0;
}

int node_check(struct node *n)
{
	int ret;
	assert(n->state != State::DESTROYED);

	ret = node_direction_check(&n->in, n);
	if (ret)
		return ret;

	ret = node_direction_check(&n->out, n);
	if (ret)
		return ret;

	ret = node_type(n)->check ? node_type(n)->check(n) : 0;
	if (ret)
		return ret;

	n->state = State::CHECKED;

	return 0;
}

int node_start(struct node *n)
{
	int ret;

	assert(n->state == State::PREPARED);
	assert(node_type(n)->state == State::STARTED);

	info("Starting node %s", node_name_long(n));

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
				serror("Failed to set FW mark for outgoing packets");
			else
				debug(LOG_SOCKET | 4, "Set FW mark for socket (sd=%u) to %u", fd, n->fwmark);
		}
	}
#endif /* __linux__ */

	n->state = State::STARTED;
	n->sequence = 0;

	return ret;
}

int node_stop(struct node *n)
{
	int ret;

	if (n->state != State::STOPPING && n->state != State::STARTED && n->state != State::CONNECTED && n->state != State::PENDING_CONNECT)
		return 0;

	info("Stopping node %s", node_name(n));

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

int node_pause(struct node *n)
{
	int ret;

	if (n->state != State::STARTED)
		return -1;

	info("Pausing node %s", node_name(n));

	ret = node_type(n)->pause ? node_type(n)->pause(n) : 0;

	if (ret == 0)
		n->state = State::PAUSED;

	return ret;
}

int node_resume(struct node *n)
{
	int ret;

	if (n->state != State::PAUSED)
		return -1;

	info("Resuming node %s", node_name(n));

	ret = node_type(n)->resume ? node_type(n)->resume(n) : 0;

	if (ret == 0)
		n->state = State::STARTED;

	return ret;
}

int node_restart(struct node *n)
{
	int ret;

	if (n->state != State::STARTED)
		return -1;

	info("Restarting node %s", node_name(n));

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

int node_destroy(struct node *n)
{
	int ret;
	assert(n->state != State::DESTROYED && n->state != State::STARTED);

	ret = node_direction_destroy(&n->in, n);
	if (ret)
		return ret;

	ret = node_direction_destroy(&n->out, n);
	if (ret)
		return ret;

	if (node_type(n)->destroy) {
		ret = (int) node_type(n)->destroy(n);
		if (ret)
			return ret;
	}

	vlist_remove_all(&node_type(n)->instances, n);

	if (n->_vd)
		free(n->_vd);

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

	n->state = State::DESTROYED;

	return 0;
}

int node_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int readd, nread = 0;

	assert(node_type(n)->read);

	if (n->state == State::PAUSED || n->state == State::PENDING_CONNECT)
		return 0;
	else if (n->state != State::STARTED && n->state != State::CONNECTED)
		return -1;

	/* Send in parts if vector not supported */
	if (node_type(n)->vectorize > 0 && node_type(n)->vectorize < cnt) {
		while (cnt - nread > 0) {
			readd = node_type(n)->read(n, &smps[nread], MIN(cnt - nread, node_type(n)->vectorize), release);
			if (readd < 0)
				return readd;

			nread += readd;
		}
	}
	else {
		nread = node_type(n)->read(n, smps, cnt, release);
		if (nread < 0)
			return nread;
	}

#ifdef WITH_HOOKS
	/* Run read hooks */
	int rread = hook_list_process(&n->in.hooks, smps, nread);
	int skipped = nread - rread;

	if (skipped > 0 && n->stats != nullptr) {
		n->stats->update(Stats::Metric::SMPS_SKIPPED, skipped);
	}

	debug(LOG_NODE | 5, "Received %u samples from node %s of which %d have been skipped", nread, node_name(n), skipped);

	return rread;
#else
	debug(LOG_NODE | 5, "Received %u samples from node %s", nread, node_name(n));

	return nread;
#endif /* WITH_HOOKS */
}

int node_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	int tosend, sent, nsent = 0;

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

	/* Send in parts if vector not supported */
	if (node_type(n)->vectorize > 0 && node_type(n)->vectorize < cnt) {
		while (cnt - nsent > 0) {
			tosend = MIN(cnt - nsent, node_type(n)->vectorize);
			sent = node_type(n)->write(n, &smps[nsent], tosend, release);
			if (sent < 0)
				return sent;

			nsent += sent;
			debug(LOG_NODE | 5, "Sent %u samples to node %s", sent, node_name(n));
		}
	}
	else {
		nsent = node_type(n)->write(n, smps, cnt, release);
		if (nsent < 0)
			return nsent;

		debug(LOG_NODE | 5, "Sent %u samples to node %s", nsent, node_name(n));
	}

	return nsent;
}

char * node_name(struct node *n)
{
	if (!n->_name)
		strcatf(&n->_name, CLR_RED("%s") "(" CLR_YEL("%s") ")", n->name, node_type_name(node_type(n)));

	return n->_name;
}

char * node_name_long(struct node *n)
{
	if (!n->_name_long) {
		if (node_type(n)->print) {
			struct node_type *vt = node_type(n);

			char uuid[37];
			uuid_unparse(n->uuid, uuid);

			strcatf(&n->_name_long, "%s: uuid=%s, #in.signals=%zu, #out.signals=%zu, #in.hooks=%zu, #out.hooks=%zu, in.vectorize=%d, out.vectorize=%d",
				node_name(n), uuid,
				vlist_length(&n->in.signals), vlist_length(&n->out.signals),
				vlist_length(&n->in.hooks),   vlist_length(&n->out.hooks),
				n->in.vectorize, n->out.vectorize
			);

#ifdef WITH_NETEM
			strcatf(&n->_name_long, ", out.netem=%s", n->tc_qdisc ? "yes" : "no");

			if (n->tc_qdisc)
				strcatf(&n->_name_long, ", fwmark=%d", n->fwmark);
#endif /* WITH_NETEM */

			/* Append node-type specific details */
			char *name_long = vt->print(n);
			strcatf(&n->_name_long, ", %s", name_long);
			free(name_long);
		}
		else
			n->_name_long = node_name(n);
	}

	return n->_name_long;
}

const char * node_name_short(struct node *n)
{
	return n->name;
}

int node_reverse(struct node *n)
{
	return node_type(n)->reverse ? node_type(n)->reverse(n) : -1;
}

int node_poll_fds(struct node *n, int fds[])
{
	return node_type(n)->poll_fds ? node_type(n)->poll_fds(n, fds) : -1;
}

int node_netem_fds(struct node *n, int fds[])
{
	return node_type(n)->netem_fds ? node_type(n)->netem_fds(n, fds) : -1;
}

struct memory_type * node_memory_type(struct node *n)
{
	return node_type(n)->memory_type ? node_type(n)->memory_type(n, memory_default) : memory_default;
}

int node_list_parse(struct vlist *list, json_t *cfg, struct vlist *all)
{
	struct node *node;
	const char *str;
	char *allstr = nullptr;

	size_t index;
	json_t *elm;

	switch (json_typeof(cfg)) {
		case JSON_STRING:
			str = json_string_value(cfg);
			node = (struct node *) vlist_lookup(all, str);
			if (!node)
				goto invalid2;

			vlist_push(list, node);
			break;

		case JSON_ARRAY:
			json_array_foreach(cfg, index, elm) {
				if (!json_is_string(elm))
					goto invalid;

				node = (struct node *) vlist_lookup(all, json_string_value(elm));
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
	error("The node list must be an a single or an array of strings referring to the keys of the 'nodes' section");

	return -1;

invalid2:
	for (size_t i = 0; i < vlist_length(all); i++) {
		struct node *n = (struct node *) vlist_at(all, i);

		strcatf(&allstr, " %s", node_name_short(n));
	}

	error("Unknown node %s. Choose of one of: %s", str, allstr);

	return 0;
}

bool node_is_valid_name(const char *name)
{
	for (const char *p = name; *p; p++) {
		if (isalnum(*p) || (*p == '_') || (*p == '-'))
			continue;

		return false;
	}

	return true;
}

bool node_is_enabled(const struct node *n)
{
	return n->enabled;
}

struct vlist * node_get_signals(struct node *n, enum NodeDir dir)
{
	struct node_direction *nd = dir == NodeDir::IN ? &n->in : &n->out;

	return node_direction_get_signals(nd);
}
