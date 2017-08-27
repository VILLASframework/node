/** API session.
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

#include "api/session.h"

#include "web.h"
#include "plugin.h"
#include "memory.h"

int api_session_init(struct api_session *s, enum api_mode m)
{
	int ret;

	s->completed = false;
	
	ret = buffer_init(&s->request.buffer, 0);
	if (ret)
		return ret;
	
	ret = buffer_init(&s->response.buffer, 0);
	if (ret)
		return ret;
	
	ret = queue_init(&s->request.queue, 128, &memtype_heap);
	if (ret)
		return ret;

	ret = queue_init(&s->response.queue, 128, &memtype_heap);
	if (ret)
		return ret;

	return 0;
}

int api_session_destroy(struct api_session *s)
{
	int ret;

	if (s->state == STATE_DESTROYED)
		return 0;

	ret = buffer_destroy(&s->request.buffer);
	if (ret)
		return ret;

	ret = buffer_destroy(&s->response.buffer);
	if (ret)
		return ret;
	
	ret = queue_destroy(&s->request.queue);
	if (ret)
		return ret;

	ret = queue_destroy(&s->response.queue);
	if (ret)
		return ret;

	s->state = STATE_DESTROYED;

	return 0;
}

int api_session_run_command(struct api_session *s, json_t *json_in, json_t **json_out)
{
	int ret;
	const char *action;
	char *id;
	struct plugin *p;

	json_t *json_args = NULL, *json_resp;

	ret = json_unpack(json_in, "{ s: s, s: s, s?: o }",
		"action", &action,
		"id", &id,
		"request", &json_args);
	if (ret) {
		ret = -100;
		*json_out = json_pack("{ s: s, s: i }",
				"error", "invalid request",
				"code", ret);
		goto out;
	}

	p = plugin_lookup(PLUGIN_TYPE_API, action);
	if (!p) {
		ret = -101;
		*json_out = json_pack("{ s: s, s: s, s: i, s: s }",
				"action", action,
				"id", id,
				"code", ret,
				"error", "command not found");
		goto out;
	}

	debug(LOG_API, "Running API request: %s", p->name);

	ret = p->api.cb(&p->api, json_args, &json_resp, s);
	if (ret)
		*json_out = json_pack("{ s: s, s: s, s: i, s: s }",
				"action", action,
				"id", id,
				"code", ret,
				"error", "command failed");
	else
		*json_out = json_pack("{ s: s, s: s, s: o }",
				"action", action,
				"id", id,
				"response", json_resp);

out:	debug(LOG_API, "API request completed with code: %d", ret);

	return 0;
}
