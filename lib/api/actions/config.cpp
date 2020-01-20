/** The "config" API ressource.
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

#include <villas/api/action.hpp>
#include <villas/api/session.hpp>
#include <villas/super_node.hpp>

namespace villas {
namespace node {
namespace api {

class ConfigAction : public Action {

public:
	using Action::Action;

	virtual int execute(json_t *args, json_t **resp)
	{
		json_t *cfg = session->getSuperNode()->getConfig();

		*resp = cfg
			? json_incref(cfg)
			: json_object();

		return 0;
	}
};

/* Register action */
static ActionPlugin<ConfigAction> p(
	"config",
	"get configuration of this VILLASnode instance"
);

} /* namespace api */
} /* namespace node */
} /* namespace villas */
