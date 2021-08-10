/** Sending statistics to another node.
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

#include <cstring>

#include <villas/node_compat.hpp>
#include <villas/nodes/stats.hpp>
#include <villas/hook.hpp>
#include <villas/stats.hpp>
#include <villas/super_node.hpp>
#include <villas/sample.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static NodeList nodes; /** The global list of nodes */

int villas::node::stats_node_signal_destroy(struct stats_node_signal *s)
{
	free(s->node_str);

	return 0;
}

int villas::node::stats_node_signal_parse(struct stats_node_signal *s, json_t *json)
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

int villas::node::stats_node_type_start(villas::node::SuperNode *sn)
{
	nodes = sn->getNodes();

	return 0;
}

int villas::node::stats_node_prepare(NodeCompat *n)
{
	auto *s = n->getData<struct stats_node>();

	assert(n->getInputSignals(false)->size() == 0);

	/* Generate signal list */
	for (size_t i = 0; i < list_length(&s->signals); i++) {
		struct stats_node_signal *stats_sig = (struct stats_node_signal *) list_at(&s->signals, i);

		const char *metric = Stats::metrics[stats_sig->metric].name;
		const char *type = Stats::types[stats_sig->type].name;

		auto name = fmt::format("{}.{}.{}", stats_sig->node_str, metric, type);

		auto sig = std::make_shared<Signal>(name.c_str(),
			Stats::metrics[stats_sig->metric].unit,
			Stats::types[stats_sig->type].signal_type);

		n->in.signals->push_back(sig);
	}

	return 0;
}

int villas::node::stats_node_start(NodeCompat *n)
{
	auto *s = n->getData<struct stats_node>();

	s->task.setRate(s->rate);

	for (size_t i = 0; i < list_length(&s->signals); i++) {
		struct stats_node_signal *stats_sig = (struct stats_node_signal *) list_at(&s->signals, i);

		stats_sig->node = nodes.lookup(stats_sig->node_str);
		if (!stats_sig->node)
			throw ConfigError(n->getConfig(), "node-config-node-stats-node", "Invalid reference node {}", stats_sig->node_str);
	}

	return 0;
}

int villas::node::stats_node_stop(NodeCompat *n)
{
	auto *s = n->getData<struct stats_node>();

	s->task.stop();

	return 0;
}

char * villas::node::stats_node_print(NodeCompat *n)
{
	auto *s = n->getData<struct stats_node>();

	return strf("rate=%f", s->rate);
}

int villas::node::stats_node_init(NodeCompat *n)
{
	int ret;
	auto *s = n->getData<struct stats_node>();

	new (&s->task) Task(CLOCK_MONOTONIC);

	ret = list_init(&s->signals);
	if (ret)
		return ret;

	return 0;
}

int villas::node::stats_node_destroy(NodeCompat *n)
{
	int ret;
	auto *s = n->getData<struct stats_node>();

	s->task.~Task();

	ret = list_destroy(&s->signals, (dtor_cb_t) stats_node_signal_destroy, true);
	if (ret)
		return ret;

	return 0;
}

int villas::node::stats_node_parse(NodeCompat *n, json_t *json)
{
	auto *s = n->getData<struct stats_node>();

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

		list_push(&s->signals, stats_sig);
	}

	return 0;
}

int villas::node::stats_node_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt)
{
	auto *s = n->getData<struct stats_node>();

	if (!cnt)
		return 0;

	s->task.wait();

	unsigned len = MIN(list_length(&s->signals), smps[0]->capacity);

	for (size_t i = 0; i < len; i++) {
		struct stats_node_signal *sig = (struct stats_node_signal *) list_at(&s->signals, i);

		auto st = sig->node->getStats();
		if (!st)
			return -1;

		smps[0]->data[i] = st->getValue(sig->metric, sig->type);
	}

	smps[0]->length = len;
	smps[0]->flags = (int) SampleFlags::HAS_DATA;
	smps[0]->signals = n->getInputSignals(false);

	return 1;
}

int villas::node::stats_node_poll_fds(NodeCompat *n, int fds[])
{
	auto *s = n->getData<struct stats_node>();

	fds[0] = s->task.getFD();

	return 0;
}

static NodeCompatType p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "stats";
	p.description	= "Send statistics to another node";
	p.vectorize	= 1;
	p.flags		= (int) NodeFactory::Flags::PROVIDES_SIGNALS;
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

	static NodeCompatFactory ncp(&p);
}
