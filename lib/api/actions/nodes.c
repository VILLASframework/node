/** The "nodes" API ressource.
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

#include <jansson.h>

#include <villas/plugin.h>
#include <villas/node.h>
#include <villas/super_node.h>
#include <villas/utils.h>
#include <villas/stats.h>

#include <villas/api.h>

static int api_nodes(struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	json_t *json_nodes = json_array();

	for (size_t i = 0; i < list_length(&s->api->super_node->nodes); i++) {
		struct node *n = (struct node *) list_at(&s->api->super_node->nodes, i);

		json_t *json_node = json_pack("{ s: s, s: i, s: i, s: i, s: i }",
			"name",		node_name_short(n),
			"state",	n->state,
			"vectorize",	n->vectorize,
			"affinity",	n->affinity,
			"id",		i
		);

		if (n->stats)
			json_object_set_new(json_node, "stats", stats_json(n->stats));

		/* Add all additional fields of node here.
		 * This can be used for metadata */
		json_object_update(json_node, n->cfg);

		json_array_append_new(json_nodes, json_node);
	}

	*resp = json_nodes;

	return 0;
}

static struct plugin p = {
	.name = "nodes",
	.description = "retrieve list of all known nodes",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_nodes
};

REGISTER_PLUGIN(&p)
