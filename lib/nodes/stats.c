/** Sending statistics to another node.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/nodes/stats.h>
#include <villas/hook.h>
#include <villas/plugin.h>
#include <villas/stats.h>
#include <villas/super_node.h>
#include <villas/sample.h>
#include <villas/node.h>

#define STATS_METRICS 6

static struct list *nodes; /** The global list of nodes */

static void stats_init_signals(struct node *n)
{
	struct stats_desc *desc;
	struct signal *sig;

	for (int i = 0; i < STATS_COUNT; i++) {
		desc = &stats_metrics[i];

		/* Total */
		sig = alloc(sizeof(struct signal));
		sig->name = strf("%s.%s", desc->name, "total");
		sig->type = SIGNAL_TYPE_INTEGER;
		list_push(&n->signals, sig);

		/* Last */
		sig = alloc(sizeof(struct signal));
		sig->name = strf("%s.%s", desc->name, "last");
		sig->unit = strdup(desc->unit);
		sig->type = SIGNAL_TYPE_FLOAT;
		list_push(&n->signals, sig);

		/* Highest */
		sig = alloc(sizeof(struct signal));
		sig->name = strf("%s.%s", desc->name, "highest");
		sig->unit = strdup(desc->unit);
		sig->type = SIGNAL_TYPE_FLOAT;
		list_push(&n->signals, sig);

		/* Lowest */
		sig = alloc(sizeof(struct signal));
		sig->name = strf("%s.%s", desc->name, "lowest");
		sig->unit = strdup(desc->unit);
		sig->type = SIGNAL_TYPE_FLOAT;
		list_push(&n->signals, sig);

		/* Mean */
		sig = alloc(sizeof(struct signal));
		sig->name = strf("%s.%s", desc->name, "mean");
		sig->unit = strdup(desc->unit);
		sig->type = SIGNAL_TYPE_FLOAT;
		list_push(&n->signals, sig);

		/* Variance */
		sig = alloc(sizeof(struct signal));
		sig->name = strf("%s.%s", desc->name, "var");
		sig->unit = strf("%s^2", desc->unit); // variance has squared unit of variable
		sig->type = SIGNAL_TYPE_FLOAT;
		list_push(&n->signals, sig);
	}
}

int stats_node_type_start(struct super_node *sn)
{
	if (!sn)
		return -1;

	nodes = &sn->nodes;

	return 0;
}

int stats_node_start(struct node *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;
	int ret;

	ret = task_init(&s->task, s->rate, CLOCK_MONOTONIC);
	if (ret)
		serror("Failed to create task");

	s->node = list_lookup(nodes, s->node_str);
	if (!s->node)
		error("Invalid reference node %s for setting 'node' of node %s", s->node_str, node_name(n));

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

	return strf("node=%s, rate=%f", s->node_str, s->rate);
}

int stats_node_parse(struct node *n, json_t *cfg)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	int ret;
	json_error_t err;

	const char *node;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s: F }",
		"node", &node,
		"rate", &s->rate
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of node %s", node_name(n));

	if (s->rate <= 0)
		error("Setting 'rate' of node %s must be positive", node_name(n));

	s->node_str = strdup(node);

	stats_init_signals(n);

	return 0;
}

int stats_node_destroy(struct node *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	if (s->node_str)
		free(s->node_str);

	return 0;
}

int stats_node_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release)
{
	struct stats_node *sn = (struct stats_node *) n->_vd;
	struct stats *s = sn->node->stats;

	if (!cnt)
		return 0;

	if (!sn->node->stats)
		return 0;

	task_wait(&sn->task);

	smps[0]->length = MIN(STATS_COUNT * 6, smps[0]->capacity);
	smps[0]->flags = SAMPLE_HAS_DATA;

	for (int i = 0; i < 6 && (i+1)*STATS_METRICS <= smps[0]->length; i++) {
		int tot = hist_total(&s->histograms[i]);

		smps[0]->data[i*STATS_METRICS+0].f = tot ? hist_total(&s->histograms[i]) : 0;
		smps[0]->data[i*STATS_METRICS+1].f = tot ? hist_last(&s->histograms[i]) : 0;
		smps[0]->data[i*STATS_METRICS+2].f = tot ? hist_highest(&s->histograms[i]) : 0;
		smps[0]->data[i*STATS_METRICS+3].f = tot ? hist_lowest(&s->histograms[i]) : 0;
		smps[0]->data[i*STATS_METRICS+4].f = tot ? hist_mean(&s->histograms[i]) : 0;
		smps[0]->data[i*STATS_METRICS+5].f = tot ? hist_var(&s->histograms[i]) : 0;
	}

	return 1;
}

int stats_node_fd(struct node *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	return task_fd(&s->task);
}

static struct plugin p = {
	.name		= "stats",
	.description	= "Send statistics to another node",
	.type		= PLUGIN_TYPE_NODE,
	.node		= {
		.vectorize	= 1,
		.flags		= NODE_TYPE_PROVIDES_SIGNALS,
		.size		= sizeof(struct stats_node),
		.type.start	= stats_node_type_start,
		.parse		= stats_node_parse,
		.destroy	= stats_node_destroy,
		.print		= stats_node_print,
		.start		= stats_node_start,
		.stop		= stats_node_stop,
		.read		= stats_node_read,
		.fd		= stats_node_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)

/** @} */
