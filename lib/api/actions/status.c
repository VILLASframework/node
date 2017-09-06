/** The "stats" API action.
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
#include "node.h"
#include "super_node.h"
#include "utils.h"
#include "stats.h"

#include "api.h"

static int api_status(struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	int ret;
	struct lws_context *ctx = lws_get_context(s->wsi);
	char buf[4096];

	ret = lws_json_dump_context(ctx, buf, sizeof(buf), 0);

	*resp = json_loads(buf, 0, NULL);

	return ret;
}

static struct plugin p = {
	.name = "status",
	.description = "get status and statistics of web server",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_status
};

REGISTER_PLUGIN(&p)
