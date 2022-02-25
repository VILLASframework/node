/** Capabilities
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

#include <villas/capabilities.hpp>

#include <villas/plugin.hpp>
#include <villas/hook.hpp>
#include <villas/format.hpp>
#include <villas/api/request.hpp>

using namespace villas;
using namespace villas::node;

json_t * villas::node::getCapabilities()
{
	json_t *json_hooks = json_array();
	json_t *json_apis  = json_array();
	json_t *json_nodes = json_array();
	json_t *json_formats = json_array();
	json_t *json_name;

	for (auto p : plugin::registry->lookup<api::RequestFactory>()) {
		json_name = json_string(p->getName().c_str());

		json_array_append_new(json_apis, json_name);
	}

	for (auto p : plugin::registry->lookup<HookFactory>()) {
		json_name = json_string(p->getName().c_str());

		json_array_append_new(json_hooks, json_name);
	}

	for (auto p : plugin::registry->lookup<FormatFactory>()) {
		json_name = json_string(p->getName().c_str());

		json_array_append_new(json_formats, json_name);
	}

	for (auto f : plugin::registry->lookup<NodeFactory>()) {
		if (f->isInternal())
			continue;

		json_name = json_string(f->getName().c_str());

		json_array_append_new(json_nodes, json_name);
	}

	return json_pack("{ s: o, s: o, s: o, s: o }",
		"hooks", json_hooks,
		"nodes", json_nodes,
		"apis", json_apis,
		"formats", json_formats);
}
