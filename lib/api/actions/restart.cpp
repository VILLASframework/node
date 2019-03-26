/** The "restart" API action.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/super_node.hpp>
#include <villas/api/action.hpp>
#include <villas/api/session.hpp>
#include <villas/log.hpp>
#include <villas/node/exceptions.hpp>
#include <villas/utils.h>

namespace villas {
namespace node {
namespace api {

class RestartAction : public Action {

protected:
	static std::string configUri;

	static void handler()
	{
		int ret;
		const char *cfg = !configUri.empty()
				    ? configUri.c_str()
				    : nullptr;

		const char *argv[] = { "villas-node", cfg, nullptr };

		Logger logger = logging.get("api");

		logger->info("Restart instance: config={}", cfg);

		ret = execvp("/proc/self/exe", (char **) argv);
		if (ret)
			throw SystemError("Failed to restart");
	}

public:
	using Action::Action;

	virtual int execute(json_t *args, json_t **resp)
	{
		int ret;
		json_error_t err;

		const char *cfg = nullptr;

		if (args) {
			ret = json_unpack_ex(args, &err, 0, "{ s?: s }", "config", &cfg);
			if (ret < 0) {
				*resp = json_string("failed to parse request");
				return -1;
			}
		}

		/* If no config is provided via request, we will use the previous one */
		configUri = cfg
			     ? cfg
			     : session->getSuperNode()->getConfigUri();

		logger->info("Restarting to %s", configUri.c_str());

		/* Increment API restart counter */
		char *scnt = getenv("VILLAS_API_RESTART_COUNT");
		int cnt = scnt ? atoi(scnt) : 0;
		char buf[32];
		snprintf(buf, sizeof(buf), "%d", cnt + 1);

		/* We pass some env variables to the new process */
		setenv("VILLAS_API_RESTART_COUNT", buf, 1);

		*resp = json_pack("{ s: i, s: o }",
			"restarts", cnt,
			"config", configUri.empty()
			            ? json_null()
				    : json_string(configUri.c_str())
		);

		/* Register exit handler */
		ret = atexit(handler);
		if (ret)
			return 0;

		/* Properly terminate current instance */
		killme(SIGTERM);

		return 0;
	}
};

std::string RestartAction::configUri;

/* Register action */
static ActionPlugin<RestartAction> p(
	"restart",
	"restart VILLASnode with new configuration"
);

} /* namespace api */
} /* namespace node */
} /* namespace villas */

