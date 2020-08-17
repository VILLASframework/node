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
#include <villas/node.h>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/api.hpp>
#include <villas/api/session.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

template<int (*A)(struct node *)>
class PathActionRequest : public Request  {

public:
	using Request::Request;

	virtual Response * execute()
	{
		if (method != Method::POST)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Path endpoints do not accept any body data");

		unsigned long pathIndex;
		const auto &pathIndexStr = matches[1].str();

		try {
			pathIndex = std::atoul(pathIndexStr);
		} catch (const std::invalid_argument &e) {
			throw BadRequest("Invalid argument");
		}

		struct vlist *paths = session->getSuperNode()->getPaths();
		struct vpath *p = (struct path *) vlist_at_safe(pathIndex);

		if (!p)
			throw Error(HTTP_STATUS_NOT_FOUND, "Node not found");

		A(p);

		return new Response(session);
	}

};

/* Register API requests */
char n1[] = "path/start";
char r1[] = "/path/([^/]+)/start";
char d1[] = "start a path";
static RequestPlugin<PathActionRequest<path_start>, n1, r1, d1> p1;

char n2[] = "path/stop";
char r2[] = "/path/([^/]+)/stop";
char d2[] = "stop a path";
static RequestPlugin<PathActionRequest<path_stop>, n2, r2, d2> p2;


} /* namespace api */
} /* namespace node */
} /* namespace villas */
