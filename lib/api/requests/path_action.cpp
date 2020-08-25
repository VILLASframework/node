/** The API ressource for start/stop/pause/resume paths.
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

#include <villas/plugin.h>
#include <villas/path.h>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/api.hpp>
#include <villas/api/session.hpp>
#include <villas/api/path_request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

template<int (*A)(struct vpath *)>
class PathActionRequest : public PathRequest  {

public:
	using PathRequest::PathRequest;

	virtual Response * execute()
	{
		if (method != Method::POST)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Path endpoints do not accept any body data");

		A(path);

		return new Response(session);
	}

};

/* Register API requests */
static char n1[] = "path/start";
static char r1[] = "/path/(" REGEX_UUID ")/start";
static char d1[] = "start a path";
static RequestPlugin<PathActionRequest<path_start>, n1, r1, d1> p1;

static char n2[] = "path/stop";
static char r2[] = "/path/(" REGEX_UUID ")/stop";
static char d2[] = "stop a path";
static RequestPlugin<PathActionRequest<path_stop>, n2, r2, d2> p2;


} /* namespace api */
} /* namespace node */
} /* namespace villas */
