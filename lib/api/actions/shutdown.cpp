/** The "shutdown" API action.
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

#include <signal.h>

#include <villas/utils.h>
#include <villas/api/action.hpp>

namespace villas {
namespace node {
namespace api {

class ShutdownAction : public Action {

public:
	using Action::Action;
	virtual int execute(json_t *args, json_t **resp)
	{
		killme(SIGTERM);

		return 0;
	}
};

/* Register action */
static ActionPlugin<ShutdownAction> p(
	"shutdown",
	"quit VILLASnode"
);

} // api
} // node
} // villas
