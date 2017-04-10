/** The "nodes" API ressource.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <jansson.h>

#include "plugin.h"
#include "node.h"
#include "utils.h"

#include "api.h"

extern struct list nodes;

static int api_nodes(struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	json_t *json_nodes = json_array();

	for (size_t i = 0; i < list_length(&s->api->super_node->nodes); i++) {
		struct node *n = list_at(&s->api->super_node->nodes, i);

		json_t *json_node = json_pack("{ s: s, s: i, s: i, s: i, s: i }",
			"name",		node_name_short(n),
			"state",	n->state,
			"vectorize",	n->vectorize,
			"affinity",	n->affinity,
			"id",		i
		);
		
		/* Add all additional fields of node here.
		 * This can be used for metadata */	
		json_object_update(json_node, config_to_json(n->cfg));
		
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