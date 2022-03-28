/** The Universal Data-exchange API.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <uuid.h>

#include <villas/api/requests/universal.hpp>
#include <villas/api/response.hpp>
#include <villas/node.hpp>

namespace villas {
namespace node {
namespace api {
namespace universal {

class InfoRequest : public UniversalRequest {
public:
	using UniversalRequest::UniversalRequest;

	virtual Response * execute()
	{
		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("This endpoint does not accept any body data");

		auto uid = node->getUuid();

		char uid_str[UUID_STR_LEN];
		uuid_unparse(uid, uid_str);

		auto *info = json_pack("{ s: s, s: s, s: { s: s, s: s, s: s } }",
			"id", node->getNameShort().c_str(),
			"uuid", uid_str,

			"transport",
				"type", "villas",
				"version", PROJECT_VERSION,
				"build", PROJECT_BUILD_ID
		);

		return new JsonResponse(session, HTTP_STATUS_OK, info);
	}
};

/* Register API requests */
static char n[] = "universal/info";
static char r[] = "/universal/(" RE_NODE_NAME ")/info";
static char d[] = "get infos of universal data-exchange API";
static RequestPlugin<InfoRequest, n, r, d> p;

} /* namespace universal */
} /* namespace api */
} /* namespace node */
} /* namespace villas */
