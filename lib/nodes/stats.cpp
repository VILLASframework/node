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

#include <villas/nodes/stats.hpp>
#include <villas/hook.hpp>
#include <villas/plugin.h>
#include <villas/stats.hpp>
#include <villas/super_node.hpp>
#include <villas/sample.h>
#include <villas/node.h>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static struct vlist *nodes; /** The global list of nodes */

int stats_node_signal_destroy(struct stats_node_signal *s)
{
	free(s->node_str);

	return 0;
}

int stats_node_signal_parse(struct stats_node_signal *s, json_t *cfg)
{
	json_error_t err;

	int ret;
	const char *stats;
	char *metric, *type, *node, *cpy, *lasts;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s }",
		"stats", &stats
	);
	if (ret)
		return -1;

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

		stats_sig->node = vlist_lookup_name<struct vnode>(nodes, stats_sig->node_str);
		if (!stats_sig->node)
			error("Invalid reference node %s for setting 'node' of node %s", stats_sig->node_str, node_name(n));
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

int stats_node_parse(struct vnode *n, json_t *cfg)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	int ret;
	size_t i;
	json_error_t err;
	json_t *json_signals, *json_signal;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: F, s: { s: o } }",
		"rate", &s->rate,
		"in",
			"signals", &json_signals
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (s->rate <= 0)
		error("Setting 'rate' of node %s must be positive", node_name(n));

	if (!json_is_array(json_signals))
		error("Setting 'in.signals' of node %s must be an array", node_name(n));

	json_array_foreach(json_signals, i, json_signal) {
		auto *stats_sig = new struct stats_node_signal;
		if (!stats_sig)
			throw MemoryAllocationError();

		ret = stats_node_signal_parse(stats_sig, json_signal);
		if (ret)
			error("Failed to parse statistics signal definition of node %s", node_name(n));

		vlist_push(&s->signals, stats_sig);
	}

	return 0;
}

int stats_node_read(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release)
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

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name			= "stats";
	p.description		= "Send statistics to another node";
	p.type			= PluginType::NODE;
	p.node.instances.state	= State::DESTROYED;
	p.node.vectorize	= 1;
	p.node.flags		= (int) NodeFlags::PROVIDES_SIGNALS;
	p.node.size		= sizeof(struct stats_node);
	p.node.type.start	= stats_node_type_start;
	p.node.parse		= stats_node_parse;
	p.node.init		= stats_node_init;
	p.node.destroy		= stats_node_destroy;
	p.node.print		= stats_node_print;
	p.node.prepare		= stats_node_prepare;
	p.node.start		= stats_node_start;
	p.node.stop		= stats_node_stop;
	p.node.read		= stats_node_read;
	p.node.poll_fds		= stats_node_poll_fds;

	int ret = vlist_init(&p.node.instances);
	if (!ret)
		vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	vlist_remove_all(&plugins, &p);
}
