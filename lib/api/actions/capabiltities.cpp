/** The "capabiltities" API ressource.
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

#include <villas/config.h>
#include <villas/api/action.hpp>

namespace villas {
namespace node {
namespace api {

class CapabilitiesAction : public Action {

public:
	using Action::Action;
	virtual int execute(json_t *args, json_t **resp)
	{
		json_t *json_hooks = json_array();
		json_t *json_apis  = json_array();
		json_t *json_nodes = json_array();
		json_t *json_formats = json_array();
		json_t *json_name;

		for (auto f : plugin::Registry::lookup<ActionFactory>()) {
			json_name = json_string(f->getName().c_str());

			json_array_append_new(json_apis, json_name);
		}

#if 0 // @todo Port to C++
		for (auto f : NodeFactory::lookup()) {
			json_name = json_string(f->getName().c_str());

			json_array_append_new(json_nodes, json_name);
		}

		for (auto f : HookFactory::lookup()) {
			json_name = json_string(f->getName().c_str());

			json_array_append_new(json_hooks, json_name);
		}

		for (auto f : FormatFactory::lookup()) {
			json_name = json_string(f->getName().c_str());

			json_array_append_new(json_formats, json_name);
		}
#endif

		*resp = json_pack("{ s: s, s: o, s: o, s: o, s: o }",
				"build", PROJECT_BUILD_ID,
				"hooks", json_hooks,
				"node-types", json_nodes,
				"apis", json_apis,
				"formats", json_formats);

		return 0;
	}
};

/* Register action */
static ActionPlugin<CapabilitiesAction> p(
	"capabilities",
	"get capabiltities and details about this VILLASnode instance"
);

} // api
} // node
} // villas
