/** API session.
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

#include <sstream>

#include <libwebsockets.h>

#include <villas/web.hpp>
#include <villas/plugin.h>
#include <villas/memory.h>

#include <villas/api/session.hpp>
#include <villas/api/action.hpp>

using namespace villas;
using namespace villas::node::api;

Logger Session::logger = logging.get("api:session");

Session::Session(Api *a) :
	runs(0),
	api(a)
{
	logger->debug("Initiated API session: {}", getName());
}

Session::~Session()
{
	logger->debug("Destroyed API session: {}", getName());
}


void Session::runPendingActions()
{
	json_t *req, *resp;

	while (!request.queue.empty()) {
		req = request.queue.pop();

		runAction(req, &resp);

		json_decref(req);

		response.queue.push(resp);
	}
}

int Session::runAction(json_t *json_in, json_t **json_out)
{
	int ret;
	const char *action;
	char *id;

	json_t *json_args = nullptr;
	json_t *json_resp = nullptr;

	ret = json_unpack(json_in, "{ s: s, s: s, s?: o }",
		"action", &action,
		"id", &id,
		"request", &json_args);
	if (ret) {
		ret = -100;
		*json_out = json_pack("{ s: s, s: i }",
				"error", "invalid request",
				"code", ret);

		logger->debug("Completed API request: action={}, id={}, code={}", action, id, ret);
		return 0;
	}

	auto acf = plugin::Registry::lookup<ActionFactory>(action);
	if (!acf) {
		ret = -101;
		*json_out = json_pack("{ s: s, s: s, s: i, s: s }",
				"action", action,
				"id", id,
				"code", ret,
				"error", "action not found");

		logger->debug("Completed API request: action={}, id={}, code={}", action, id, ret);
		return 0;
	}

	logger->debug("Running API request: action={}, id={}", action, id);

	Action *act = acf->make(this);

	ret = act->execute(json_args, &json_resp);
	if (ret)
		*json_out = json_pack("{ s: s, s: s, s: i, s: s }",
				"action", action,
				"id", id,
				"code", ret,
				"error", "action failed");
	else
		*json_out = json_pack("{ s: s, s: s }",
				"action", action,
				"id", id);

	if (json_resp)
		json_object_set_new(*json_out, "response", json_resp);

	logger->debug("Completed API request: action={}, id={}, code={}", action, id, ret);

	runs++;

	return 0;
}

std::string Session::getName()
{
	std::stringstream ss;

	ss << "version=" << version << ", runs=" << runs;

	return ss.str();
}
