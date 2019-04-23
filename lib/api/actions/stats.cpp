/** The API ressource for getting and resetting statistics.
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

#include <villas/log.h>
#include <villas/node.h>
#include <villas/stats.h>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/api/session.hpp>
#include <villas/api/action.hpp>
#include <villas/api.hpp>

namespace villas {
namespace node {
namespace api {

class StatsAction : public Action  {

public:
	using Action::Action;

	virtual int execute(json_t *args, json_t **resp)
	{
		int ret, reset = 0;
		json_error_t err;

		ret = json_unpack_ex(args, &err, 0, "{ s?: b }",
			"reset", &reset
		);
		if (ret < 0)
			return ret;

		struct vlist *nodes = session->getSuperNode()->getNodes();

		for (size_t i = 0; i < vlist_length(nodes); i++) {
			struct node *n = (struct node *) vlist_at(nodes, i);

			if (n->stats) {
				if (reset) {
					stats_reset(n->stats);
					info("Stats resetted for node %s", node_name(n));
				}
			}
		}

		*resp = json_object();

		return 0;
	}

};

/* Register actions */
static ActionPlugin<StatsAction>    p1("stats", "get or reset internal statistics counters");

} /* namespace api */
} /* namespace node */
} /* namespace villas */
