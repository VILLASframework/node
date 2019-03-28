/** The API ressource for start/stop/pause/resume nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/utils.h>
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
static ActionPlugin<NodeAction<node_start>>    p1("node.start",   "start a node");
static ActionPlugin<NodeAction<node_stop>>     p2("node.stop",    "stop a node");
static ActionPlugin<NodeAction<node_pause>>    p3("node.pause",   "pause a node");
static ActionPlugin<NodeAction<node_resume>>   p4("node.resume",  "resume a node");
static ActionPlugin<NodeAction<node_restart>>  p5("node.restart", "restart a node");

} /* namespace api */
} /* namespace node */
} /* namespace villas */
