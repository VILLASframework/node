/** Capabilities
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
