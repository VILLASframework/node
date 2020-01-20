/** The "paths" API ressource.
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

#include <villas/super_node.hpp>
#include <villas/path.h>
#include <villas/utils.hpp>
#include <villas/api/action.hpp>
#include <villas/api/session.hpp>

namespace villas {
namespace node {
namespace api {

class PathsAction : public Action {

public:
	using Action::Action;

	virtual int execute(json_t *args, json_t **resp)
	{
		json_t *json_paths = json_array();

		struct vlist *paths = session->getSuperNode()->getPaths();

		for (size_t i = 0; i < vlist_length(paths); i++) {
			struct path *p = (struct path *) vlist_at(paths, i);

			json_t *json_path = json_pack("{ s: i }",
				"state",	p->state
			);

			/* Add all additional fields of node here.
			 * This can be used for metadata */
			json_object_update(json_path, p->cfg);

			json_array_append_new(json_paths, json_path);
		}

		*resp = json_paths;

		return 0;
	}
};

/* Register action */
static ActionPlugin<PathsAction> p(
	"paths",
	"retrieve list of all paths with details"
);

} /* namespace api */
} /* namespace node */
} /* namespace villas */
