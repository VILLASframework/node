/** The "file" API ressource.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <villas/super_node.hpp>
#include <villas/api/session.hpp>
#include <villas/api/node_request.hpp>
#include <villas/api/response.hpp>

#include <villas/node_compat.hpp>
#include <villas/nodes/file.hpp>
#include <villas/format.hpp>

namespace villas {
namespace node {
namespace api {

class FileRequest : public NodeRequest {

public:
	using NodeRequest::NodeRequest;

	virtual Response * execute()
	{
		if (method != Session::Method::GET && method != Session::Method::POST)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("File endpoint does not accept any body data");

		NodeFactory *nf = plugin::registry->lookup<NodeFactory>("file");

		if (node->getFactory() != nf)
			throw BadRequest("This node is not a file node", "{ s: s }",
				"type", node->getFactory()->getName()
			);

		auto *nc = dynamic_cast<NodeCompat *>(node);
		auto *f = nc->getData<struct file>();

		if (matches[2] == "rewind")
			rewind(f->stream_in);

		return new Response(session, HTTP_STATUS_OK);
	}
};

/* Register API request */
static char n[] = "node/file";
static char r[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/file(?:/([^/]+))?";
static char d[] = "control instances of 'file' node-type";
static RequestPlugin<FileRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
