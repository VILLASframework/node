/** The API ressource for start/stop/pause/resume nodes.
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
#include <villas/api/node_request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

template<int (*A)(struct vnode *)>
class NodeActionRequest : public NodeRequest  {

public:
	using NodeRequest::NodeRequest;

	virtual Response * execute()
	{
		if (method != Session::Method::POST)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Node endpoints do not accept any body data");

		A(node);

		return new Response(session);
	}

};

/* Register API requests */
static char n1[] = "node/start";
static char r1[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/start";
static char d1[] = "start a node";
static RequestPlugin<NodeActionRequest<node_start>, n1, r1, d1> p1;

static char n2[] = "node/stop";
static char r2[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/stop";
static char d2[] = "stop a node";
static RequestPlugin<NodeActionRequest<node_stop>, n2, r2, d2> p2;

static char n3[] = "node/pause";
static char r3[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/pause";
static char d3[] = "pause a node";
static RequestPlugin<NodeActionRequest<node_pause>, n3, r3, d3> p3;

static char n4[] = "node/resume";
static char r4[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/resume";
static char d4[] = "resume a node";
static RequestPlugin<NodeActionRequest<node_resume>, n4, r4, d4> p4;

static char n5[] = "node/restart";
static char r5[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/restart";
static char d5[] = "restart a node";
static RequestPlugin<NodeActionRequest<node_restart>, n5, r5, d5> p5;


} /* namespace api */
} /* namespace node */
} /* namespace villas */
