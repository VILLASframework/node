/** The "restart" API action.
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

#include "plugin.h"
#include "api.h"
#include "super_node.h"

#include "log.h"

static char *config;

void api_restart_handler()
{
	int ret;
	
	char *argv[] = { "villas-node", config, NULL };

	ret = execvpe("/proc/self/exe", argv, environ);
	if (ret)
		serror("Failed to restart");
}

static int api_restart(struct api_action *h, json_t *args, json_t **resp, struct api_session *s)
{
	int ret;
	json_error_t err;
	
	/* If no config is provided via request, we will use the previous one */
	config = strdup(s->api->super_node->uri);
	
	if (args) {
		ret = json_unpack_ex(args, &err, 0, "{ s?: s }", "config", &config);
		if (ret < 0) {
			*resp = json_string("failed to parse request");
			return -1;
		}
	}

	/* Increment API restart counter */
	char *scnt = getenv("VILLAS_API_RESTART_COUNT");
	int cnt = scnt ? atoi(scnt) : 0;
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", cnt + 1);

	/* We pass some env variables to the new process */
	setenv("VILLAS_API_RESTART_COUNT", buf, 1);
	setenv("VILLAS_API_RESTART_REMOTE", s->peer.ip, 1);
	
	*resp = json_pack("{ s: i, s: s, s: s }",
		"restarts", cnt,
		"config", config,
		"remote", s->peer.ip
	);

	/* Register exit handler */
	ret = atexit(api_restart_handler);
	if (ret)
		return 0;
	
	/* Properly terminate current instance */
	killme(SIGTERM);

	return 0;
}

static struct plugin p = {
	.name = "restart",
	.description = "restart VILLASnode with new configuration",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_restart
};

REGISTER_PLUGIN(&p)
