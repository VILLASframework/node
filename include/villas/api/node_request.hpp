/** API Request for nodes.
 *
 * @file
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

#include <uuid/uuid.h>

#include <villas/api/request.hpp>

/* Forward declarations */
struct vnode;

namespace villas {
namespace node {
namespace api {

class NodeRequest : public Request {

public:
	using Request::Request;

	struct vnode *node;

	virtual void
	prepare()
	{
		int ret;

		auto *nodes = session->getSuperNode()->getNodes();

		uuid_t uuid;
		ret = uuid_parse(matches[1].str().c_str(), uuid);
		if (ret) {
			node = vlist_lookup_name<struct vnode>(nodes, matches[1].str());
			if (!node)
				throw BadRequest("Unknown node", "{ s: s }",
					"node", matches[1].str().c_str()
				);
		}
		else {
			node = vlist_lookup_uuid<struct vnode>(nodes, uuid);
			if (!node)
				throw BadRequest("No node found with with matching UUID", "{ s: s }",
					"uuid", matches[1].str().c_str()
				);
		}
	}
};

} /* namespace api */
} /* namespace node */
} /* namespace villas */
