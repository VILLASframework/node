/** Sending statistics to another node.
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

#include <cstring>

#include <villas/node.h>
#include <villas/nodes/stats.hpp>
#include <villas/hook.hpp>
#include <villas/stats.hpp>
#include <villas/super_node.hpp>
#include <villas/sample.h>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static NodeList nodes; /** The global list of nodes */

int stats_node_signal_destroy(struct stats_node_signal *s)
{
	free(s->node_str);

	return 0;
}

int stats_node_signal_parse(struct stats_node_signal *s, json_t *json)
{
	json_error_t err;

	int ret;
	const char *stats;
	char *metric, *type, *node, *cpy, *lasts;

	ret = json_unpack_ex(json, &err, 0, "{ s: s }",
		"stats", &stats
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-stats");

	cpy = strdup(stats);

	node = strtok_r(cpy, ".", &lasts);
	if (!node)
		goto invalid_format;

	metric = strtok_r(nullptr, ".", &lasts);
	if (!metric)
		goto invalid_format;

	type = strtok_r(nullptr, ".", &lasts);
	if (!type)
		goto invalid_format;

	s->metric = Stats::lookupMetric(metric);
	s->type = Stats::lookupType(type);

	s->node_str = strdup(node);

	free(cpy);
	return 0;

invalid_format:
	free(cpy);
	return -1;
}

int stats_node_type_start(villas::node::SuperNode *sn)
{
	nodes = sn->getNodes();

	return 0;
}

int stats_node_prepare(struct vnode *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	assert(vlist_length(&n->in.signals) == 0);

	/* Generate signal list */
	for (size_t i = 0; i < vlist_length(&s->signals); i++) {
		struct stats_node_signal *stats_sig = (struct stats_node_signal *) vlist_at(&s->signals, i);
		struct signal *sig;

		const char *metric = Stats::metrics[stats_sig->metric].name;
		const char *type = Stats::types[stats_sig->type].name;

		auto name = fmt::format("{}.{}.{}", stats_sig->node_str, metric, type);

		sig = signal_create(name.c_str(),
			Stats::metrics[stats_sig->metric].unit,
			Stats::types[stats_sig->type].signal_type);

		vlist_push(&n->in.signals, sig);
	}

	return 0;
}

int stats_node_start(struct vnode *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	s->task.setRate(s->rate);

	for (size_t i = 0; i < vlist_length(&s->signals); i++) {
		struct stats_node_signal *stats_sig = (struct stats_node_signal *) vlist_at(&s->signals, i);

		stats_sig->node = nodes.lookup(stats_sig->node_str);
		if (!stats_sig->node)
			throw ConfigError(n->config, "node-config-node-stats-node", "Invalid reference node {}", stats_sig->node_str);
	}

	return 0;
}

int stats_node_stop(struct vnode *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	s->task.stop();

	return 0;
}

char * stats_node_print(struct vnode *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	return strf("rate=%f", s->rate);
}

int stats_node_init(struct vnode *n)
{
	int ret;
	struct stats_node *s = (struct stats_node *) n->_vd;

	new (&s->task) Task(CLOCK_MONOTONIC);

	ret = vlist_init(&s->signals);
	if (ret)
		return ret;

	return 0;
}

int stats_node_destroy(struct vnode *n)
{
	int ret;
	struct stats_node *s = (struct stats_node *) n->_vd;

	s->task.~Task();

	ret = vlist_destroy(&s->signals, (dtor_cb_t) stats_node_signal_destroy, true);
	if (ret)
		return ret;

	return 0;
}

int stats_node_parse(struct vnode *n, json_t *json)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	int ret;
	size_t i;
	json_error_t err;
	json_t *json_signals, *json_signal;

	ret = json_unpack_ex(json, &err, 0, "{ s: F, s: { s: o } }",
		"rate", &s->rate,
		"in",
			"signals", &json_signals
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-stats");

	if (s->rate <= 0)
		throw ConfigError(json, "node-config-node-stats-rate", "Setting 'rate' must be positive");

	if (!json_is_array(json_signals))
		throw ConfigError(json, "node-config-node-stats-in-signals", "Setting 'in.signals' must be an array");

	json_array_foreach(json_signals, i, json_signal) {
		auto *stats_sig = new struct stats_node_signal;
		if (!stats_sig)
			throw MemoryAllocationError();

		ret = stats_node_signal_parse(stats_sig, json_signal);
		if (ret)
			throw ConfigError(json_signal, "node-config-node-stats-signals", "Failed to parse statistics signal definition");

		vlist_push(&s->signals, stats_sig);
	}

	return 0;
}

int stats_node_read(struct vnode *n, struct sample * const smps[], unsigned cnt)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	if (!cnt)
		return 0;

	s->task.wait();

	unsigned len = MIN(vlist_length(&s->signals), smps[0]->capacity);

	for (size_t i = 0; i < len; i++) {
		struct stats_node_signal *sig = (struct stats_node_signal *) vlist_at(&s->signals, i);

		auto st = sig->node->stats;
		if (!st)
			return -1;

		smps[0]->data[i] = st->getValue(sig->metric, sig->type);
	}

	smps[0]->length = len;
	smps[0]->flags = (int) SampleFlags::HAS_DATA;
	smps[0]->signals = &n->in.signals;

	return 1;
}

int stats_node_poll_fds(struct vnode *n, int fds[])
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	fds[0] = s->task.getFD();

	return 0;
}

static struct vnode_type p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "stats";
	p.description	= "Send statistics to another node";
	p.vectorize	= 1;
	p.flags		= (int) NodeFlags::PROVIDES_SIGNALS;
	p.size		= sizeof(struct stats_node);
	p.type.start	= stats_node_type_start;
	p.parse		= stats_node_parse;
	p.init		= stats_node_init;
	p.destroy	= stats_node_destroy;
	p.print		= stats_node_print;
	p.prepare	= stats_node_prepare;
	p.start		= stats_node_start;
	p.stop		= stats_node_stop;
	p.read		= stats_node_read;
	p.poll_fds	= stats_node_poll_fds;

	if (!node_types)
		node_types = new NodeTypeList();

	node_types->push_back(&p);
}
