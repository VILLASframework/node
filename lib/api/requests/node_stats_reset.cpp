/** The API ressource for resetting statistics.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/node.hpp>
#include <villas/stats.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/api.hpp>
#include <villas/api/session.hpp>
#include <villas/api/node_request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class StatsRequest : public NodeRequest  {

public:
	using NodeRequest::NodeRequest;

	virtual Response * execute()
	{
		if (method != Session::Method::POST)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Stats endpoint does not accept any body data");

		if (node->getStats() == nullptr)
			throw BadRequest("The statistics collection for this node is not enabled");

		node->getStats()->reset();

		return new Response(session, HTTP_STATUS_OK);
	}
};

/* Register API requests */
static char n[] = "node/stats/reset";
static char r[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/stats/reset";
static char d[] = "reset internal statistics counters";
static RequestPlugin<StatsRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
