/** The API ressource for resetting statistics.
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

#include <villas/log.h>
#include <villas/node.h>
#include <villas/stats.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/api.hpp>
#include <villas/api/session.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class StatsRequest : public Request  {

public:
	using Request::Request;

	virtual Response * execute()
	{
		int ret;
		json_error_t err;

		if (method != Method::POST)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Stats endpoint does not accept any body data");


		char *node = nullptr;

		ret = json_unpack_ex(body, &err, 0, "{ s?: s }",
			"node", &node
		);
		if (ret < 0)
			throw Error();

		struct vlist *nodes = session->getSuperNode()->getNodes();

		for (size_t i = 0; i < vlist_length(nodes); i++) {
			struct vnode *n = (struct vnode *) vlist_at(nodes, i);

			if (node && strcmp(node, node_name(n)))
				continue;

			if (n->stats) {
				n->stats->reset();
				info("Stats resetted for node %s", node_name(n));
			}
		}

		return new Response(session);
	}
};

/* Register API requests */
static char n[] = "node/stats/reset";
static char r[] = "/node/(" REGEX_NODE_NAME "|" REGEX_UUID ")/stats/reset";
static char d[] = "reset internal statistics counters";
static RequestPlugin<StatsRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
