/** The "capabiltities" API ressource.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/plugin.h>
#include <villas/config.h>

static int api_capabilities(struct api_action *h, json_t *args, json_t **resp, struct api_session *s)
{
	json_t *json_hooks = json_array();
	json_t *json_apis  = json_array();
	json_t *json_nodes = json_array();
	json_t *json_ios   = json_array();
	json_t *json_name;

	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *p = (struct plugin *) list_at(&plugins, i);

		json_name = json_string(p->name);

		switch (p->type) {
			case PLUGIN_TYPE_NODE:
				json_array_append_new(json_nodes, json_name);
				break;

			case PLUGIN_TYPE_HOOK:
				json_array_append_new(json_hooks, json_name);
				break;

			case PLUGIN_TYPE_API:
				json_array_append_new(json_apis, json_name);
				break;

			case PLUGIN_TYPE_FORMAT:
				json_array_append_new(json_ios,  json_name);
				break;

			default:
				json_decref(json_name);
		}
	}

	*resp = json_pack("{ s: s, s: o, s: o, s: o, s: o }",
			"build", BUILDID,
			"hooks",      json_hooks,
			"node-types", json_nodes,
			"apis",       json_apis,
			"formats",    json_ios);

	return 0;
}

static struct plugin p = {
	.name = "capabilities",
	.description = "get capabiltities and details about this VILLASnode instance",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_capabilities
};

REGISTER_PLUGIN(&p)
