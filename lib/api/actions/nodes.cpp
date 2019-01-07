/** The "nodes" API ressource.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/super_node.hpp>
#include <villas/node.h>
#include <villas/utils.h>
#include <villas/stats.h>
#include <villas/api/action.hpp>
#include <villas/api/session.hpp>

namespace villas {
namespace node {
namespace api {

class NodesAction : public Action {

public:
	using Action::Action;
	virtual int execute(json_t *args, json_t **resp)
	{
		json_t *json_nodes = json_array();

		struct vlist *nodes = session->getSuperNode()->getNodes();

		for (size_t i = 0; i < vlist_length(nodes); i++) {
			struct node *n = (struct node *) vlist_at(nodes, i);

			json_t *json_node = json_pack("{ s: s, s: s, s: i, s: { s: i }, s: { s: i } }",
				"name",		node_name_short(n),
				"state",	state_print(n->state),
				"affinity",	n->affinity,
				"in",
					"vectorize",	n->in.vectorize,
				"out",
					"vectorize",	n->out.vectorize
			);

			if (n->stats)
				json_object_set_new(json_node, "stats", stats_json(n->stats));

			/* Add all additional fields of node here.
			 * This can be used for metadata */
			json_object_update(json_node, n->cfg);

			json_array_append_new(json_nodes, json_node);
		}

		*resp = json_nodes;

		return 0;
	}
};

/* Register action */
static ActionPlugin<NodesAction> p(
	"nodes",
	"retrieve list of all known nodes"
);

} // api
} // node
} // villas

