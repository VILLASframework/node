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

#include "nodes/stats.h"
#include "hook.h"
#include "plugin.h"
#include "stats.h"
#include "super_node.h"
#include "sample.h"
#include "node.h"

#define STATS_METRICS 6

static struct list *nodes; /** The global list of nodes */

int stats_node_init(struct super_node *sn)
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

	n->samplelen = STATS_COUNT * STATS_METRICS;

	return 0;
}

int stats_node_destroy(struct node *n)
{
	struct stats_node *s = (struct stats_node *) n->_vd;

	if (s->node_str)
		free(s->node_str);

	return 0;
}

int stats_node_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct stats_node *sn = (struct stats_node *) n->_vd;
	struct stats *s = sn->node->stats;

	if (!cnt)
		return 0;

	if (!sn->node->stats)
		return 0;

	task_wait(&sn->task);

	smps[0]->length = MIN(STATS_COUNT * 6, smps[0]->capacity);
	smps[0]->flags = SAMPLE_HAS_VALUES;

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
		.vectorize = 1,
		.size	= sizeof(struct stats_node),
		.init	= stats_node_init,
		.parse	= stats_node_parse,
		.destroy= stats_node_destroy,
		.print	= stats_node_print,
		.start	= stats_node_start,
		.stop	= stats_node_stop,
		.read	= stats_node_read,
		.fd	= stats_node_fd
	}
};

REGISTER_PLUGIN(&p)
LIST_INIT_STATIC(&p.node.instances)

/** @} */
