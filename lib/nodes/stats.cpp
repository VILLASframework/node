/** Sending statistics to another node.
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

/** @addtogroup hooks Hook functions
 * @{
 */

#include <string.h>

#include <villas/nodes/stats.hpp>
#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/stats.h>
#include <villas/super_node.h>
#include <villas/sample.h>
#include <villas/node.h>

#define STATS_METRICS 6

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

	s->metric = stats_lookup_metric(metric);
	if (s->metric < 0)
		goto invalid_format;

	s->type = stats_lookup_type(type);
	if (s->type < 0)
		goto invalid_format;

	s->node_str = strdup(node);

	free(cpy);
	return 0;

invalid_format:
	free(cpy);
	return -1;
}

int stats_node_type_start(struct super_node *sn)
{
	nodes = super_node_get_nodes(sn);

	return 0;
}

int stats_node_start(struct node *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;
	int ret;

	ret = task_init(&s->task, s->rate, CLOCK_MONOTONIC);
	if (ret)
		serror("Failed to create task");

	for (size_t i = 0; i < vlist_length(&s->signals); i++) {
		struct stats_node_signal *stats_sig = (struct stats_node_signal *) vlist_at(&s->signals, i);

		stats_sig->node = (struct node *) vlist_lookup(nodes, stats_sig->node_str);
		if (!stats_sig->node)
			error("Invalid reference node %s for setting 'node' of node %s", stats_sig->node_str, node_name(n));
	}

	return 0;
}

int stats_node_stop(struct node *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;
	int ret;

	ret = task_destroy(&s->task);
	if (ret)
		return ret;

	return 0;
}

char * stats_node_print(struct node *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	return strf("rate=%f", s->rate);
}

int stats_node_init(struct node *n)
{
	int ret;
	struct stats_node *s = (struct stats_node *) n->_vd;

	ret = vlist_init(&s->signals);
	if (ret)
		return ret;

	return 0;
}

int stats_node_destroy(struct node *n)
{
	int ret;
	struct stats_node *s = (struct stats_node *) n->_vd;

	ret = vlist_destroy(&s->signals, (dtor_cb_t) stats_node_signal_destroy, true);
	if (ret)
		return ret;

	return 0;
}

int stats_node_parse(struct node *n, json_t *cfg)
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
		struct signal *sig = (struct signal *) vlist_at(&n->in.signals, i);
		struct stats_node_signal *stats_sig;

		stats_sig = (struct stats_node_signal *) alloc(sizeof(struct stats_node_signal));
		if (!stats_sig)
			return -1;

		ret = stats_node_signal_parse(stats_sig, json_signal);
		if (ret)
			error("Failed to parse statistics signal definition of node %s", node_name(n));

		if (!sig->name) {
			const char *metric = stats_metrics[stats_sig->metric].name;
			const char *type = stats_types[stats_sig->type].name;

			sig->name = strf("%s.%s.%s", stats_sig->node_str, metric, type);
		}

		if (!sig->unit)
			sig->unit = strdup(stats_metrics[stats_sig->metric].unit);

		if (sig->type != stats_types[stats_sig->type].signal_type)
			error("Invalid type for signal %zu in node %s", i, node_name(n));

		vlist_push(&s->signals, stats_sig);
	}

	return 0;
}

int stats_node_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	if (!cnt)
		return 0;

	task_wait(&s->task);

	unsigned len = MIN(vlist_length(&s->signals), smps[0]->capacity);

	for (size_t i = 0; i < len; i++) {
		struct stats *st;
		struct stats_node_signal *sig = (struct stats_node_signal *) vlist_at(&s->signals, i);

		st = sig->node->stats;
		if (!st)
			return -1;

		smps[0]->data[i] = stats_get_value(st, sig->metric, sig->type);
	}

	smps[0]->length = len;
	smps[0]->flags = SAMPLE_HAS_DATA;
	smps[0]->signals = &n->in.signals;

	return 1;
}

int stats_node_poll_fds(struct node *n, int fds[])
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	fds[0] = task_fd(&s->task);

	return 0;
}

static struct plugin p;

__attribute__((constructor(110)))
static void register_plugin() {
	p.name		= "stats";
	p.description	= "Send statistics to another node";
	p.type		= PLUGIN_TYPE_NODE;
	p.node.vectorize	= 1;
	p.node.flags		= 0;
	p.node.size		= sizeof(struct stats_node);
	p.node.type.start	= stats_node_type_start;
	p.node.parse		= stats_node_parse;
	p.node.init		= stats_node_init;
	p.node.destroy	= stats_node_destroy;
	p.node.print		= stats_node_print;
	p.node.start		= stats_node_start;
	p.node.stop		= stats_node_stop;
	p.node.read		= stats_node_read;
	p.node.poll_fds	= stats_node_poll_fds;

	vlist_push(&plugins, &p);
}

__attribute__((destructor(110)))
static void deregister_plugin() {
	if (plugins.state != STATE_DESTROYED)
		vlist_remove_all(&plugins, &p);
}

/** @} */
