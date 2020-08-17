/** The "stats" API request.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#ifdef LWS_WITH_SERVER_STATUS

#include <jansson.h>

#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class StatusRequest : public Request {

public:
	using Request::Request;

	virtual Response * execute()
	{
		int ret;
		struct lws_context *ctx = lws_get_context(s->wsi);
		char buf[4096];

		if (method != Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Status endpoint does not accept any body data");

		ret = lws_json_dump_context(ctx, buf, sizeof(buf), 0);
		if (ret)
			throw Error();

		auto *json_status = json_loads(buf, 0, nullptr);

		return new Response(session, json_status);
	}
};

/* Register API request */
static char n[] = "status";
static char r[] = "/status";
static char d[] = "get status and statistics of web server";
static RequestPlugin<StatusRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */

#endif /* LWS_WITH_SERVER_STATUS */
