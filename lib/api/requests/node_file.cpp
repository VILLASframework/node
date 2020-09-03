/** The "file" API ressource.
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

#include <villas/super_node.hpp>
#include <villas/api/session.hpp>
#include <villas/api/node_request.hpp>
#include <villas/api/response.hpp>

#include <villas/nodes/file.hpp>
#include <villas/io.h>

namespace villas {
namespace node {
namespace api {

class FileRequest : public NodeRequest {

public:
	using NodeRequest::NodeRequest;

	virtual Response * execute()
	{
		if (method != Method::GET && method != Method::POST)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("File endpoint does not accept any body data");

		struct vnode_type *vt = node_type_lookup("file");

		if (node->_vt != vt)
			throw BadRequest("This node is not a file node");

		struct file *f = (struct file *) node->_vd;

		if (matches[2].str() == "rewind")
			io_rewind(&f->io);

		return new Response(session);
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
