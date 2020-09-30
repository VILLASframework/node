/** The "path" API ressource.
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

#include <jansson.h>
#include <uuid/uuid.h>

#include <villas/super_node.hpp>
#include <villas/path.h>
#include <villas/utils.hpp>
#include <villas/stats.hpp>
#include <villas/api/session.hpp>
#include <villas/api/path_request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class PathInfoRequest : public PathRequest {

public:
	using PathRequest::PathRequest;

	virtual Response * execute()
	{
		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Endpoint does not accept any body data");

		return new Response(session, path_to_json(path));
	}
};

/* Register API request */
static char n[] = "path";
static char r[] = "/path/(" RE_UUID ")";
static char d[] = "retrieve info of a path";
static RequestPlugin<PathInfoRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
