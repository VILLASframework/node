/** The "capabiltities" API ressource.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include "plugin.h"

static int api_capabilities(struct api_action *h, json_t *args, json_t **resp, struct api_session *s)
{
	json_t *json_hooks = json_array();
	json_t *json_apis  = json_array();
	json_t *json_nodes = json_array();
	json_t *json_name;
	
	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *p = list_at(&plugins, i);
		
		json_name = json_string(p->name);

		switch (p->type) {
			case PLUGIN_TYPE_NODE: json_array_append_new(json_nodes, json_name); break;
			case PLUGIN_TYPE_HOOK: json_array_append_new(json_hooks, json_name); break;
			case PLUGIN_TYPE_API:  json_array_append_new(json_apis, json_name); break;
			default: { }
		}
	}
	
	*resp = json_pack("{ s: s, s: o, s: o, s: o }",
			"build", BUILDID,
			"hooks",      json_hooks,
			"node-types", json_nodes,
			"apis",       json_apis);
	
	return 0;
}

static struct plugin p = {
	.name = "capabilities",
	.description = "get capabiltities and details about this VILLASnode instance",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_capabilities
};

REGISTER_PLUGIN(&p)