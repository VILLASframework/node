/** The "paths" API ressource.
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

#include "plugin.h"
#include "path.h"
#include "utils.h"
#include "config_helper.h"
#include "stats.h"
#include "super_node.h"

#include "api.h"

static int api_paths(struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	json_t *json_paths = json_array();

	for (size_t i = 0; i < list_length(&s->api->super_node->paths); i++) {
		struct path *p = list_at(&s->api->super_node->paths, i);

		json_t *json_path = json_pack("{ s: i }",
			"state",	p->state
		);

		if (p->stats)
			json_object_set(json_path, "stats", stats_json(p->stats));

		/* Add all additional fields of node here.
		 * This can be used for metadata */
		json_object_update(json_path, p->cfg);

		json_array_append_new(json_paths, json_path);
	}

	*resp = json_paths;

	return 0;
}

static struct plugin p = {
	.name = "paths",
	.description = "retrieve list of all paths with details",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_paths
};

REGISTER_PLUGIN(&p)
