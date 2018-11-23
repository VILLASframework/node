/** The API ressource for start/stop/pause/resume nodes.
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

#include <jansson.h>

#include <villas/plugin.h>
#include <villas/node.h>
#include <villas/super_node.h>
#include <villas/utils.h>
#include <villas/stats.h>

#include <villas/api.h>

enum api_control_action {
	API_NODE_START,
	API_NODE_STOP,
	API_NODE_PAUSE,
	API_NODE_RESUME,
	API_NODE_RESTART
};

static int api_control(enum api_control_action act, struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	int ret;
	json_error_t err;
	const char *node_str;

	ret = json_unpack_ex(args, &err, 0, "{ s: s }",
		"node", &node_str
	);
	if (ret)
		return ret;

	struct list *nodes = &s->api->super_node->nodes;
	struct node *node = list_lookup(nodes, node_str);

	if (!node)
		return -1;

	switch (act) {
		case API_NODE_START:
			return node_start(node);

		case API_NODE_STOP:
			return node_stop(node);

		case API_NODE_PAUSE:
			return node_pause(node);

		case API_NODE_RESUME:
			return node_resume(node);

		case API_NODE_RESTART:
			return node_restart(node);

		default:
			return -1;
	}

	return 0;
}

static int api_control_start(struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	return api_control(API_NODE_START, r, args, resp, s);
}

static int api_control_stop(struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	return api_control(API_NODE_STOP, r, args, resp, s);
}

static int api_control_pause(struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	return api_control(API_NODE_PAUSE, r, args, resp, s);
}

static int api_control_resume(struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	return api_control(API_NODE_RESUME, r, args, resp, s);
}

static int api_control_restart(struct api_action *r, json_t *args, json_t **resp, struct api_session *s)
{
	return api_control(API_NODE_RESTART, r, args, resp, s);
}

static struct plugin p1 = {
	.name = "node.start",
	.description = "start a node",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_control_start
};

static struct plugin p2 = {
	.name = "node.stop",
	.description = "stop a node",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_control_stop
};

static struct plugin p3 = {
	.name = "node.pause",
	.description = "pause a node",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_control_pause
};

static struct plugin p4 = {
	.name = "node.resume",
	.description = "resume a node",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_control_resume
};

static struct plugin p5 = {
	.name = "node.restart",
	.description = "restart a node",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_control_restart
};

REGISTER_PLUGIN(&p1)
REGISTER_PLUGIN(&p2)
REGISTER_PLUGIN(&p3)
REGISTER_PLUGIN(&p4)
REGISTER_PLUGIN(&p5)
