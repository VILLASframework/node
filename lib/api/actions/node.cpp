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
#include <villas/api/session.hpp>
#include <villas/api/action.hpp>
#include <villas/api.hpp>

namespace villas {
namespace node {
namespace api {

template<int (*A)(struct node *)>
class NodeAction : public Action  {

public:
	using Action::Action;

	virtual int execute(json_t *args, json_t **resp)
	{
		int ret;
		json_error_t err;
		const char *node_str;

		ret = json_unpack_ex(args, &err, 0, "{ s: s }",
			"node", &node_str
		);
		if (ret < 0)
			return ret;

		struct vlist *nodes = session->getSuperNode()->getNodes();
		struct node *n = (struct node *) vlist_lookup(nodes, node_str);

		if (!n)
			return -1;

		return A(n);
	}

};

/* Register actions */
char n1[] = "node.start";
char d1[] = "start a node";
static ActionPlugin<NodeAction<node_start>,   n1, d1> p1;

char n2[] = "node.stop";
char d2[] = "stop a node";
static ActionPlugin<NodeAction<node_stop>,    n2, d2> p2;

char n3[] = "node.pause";
char d3[] = "pause a node";
static ActionPlugin<NodeAction<node_pause>,   n3, d3> p3;

char n4[] = "node.resume";
char d4[] = "resume a node";
static ActionPlugin<NodeAction<node_resume>,  n4, d4> p4;

char n5[] = "node.restart";
char d5[] = "restart a node";
static ActionPlugin<NodeAction<node_restart>, n5, d5> p5;


} /* namespace api */
} /* namespace node */
} /* namespace villas */
