/** The "capabiltities" API ressource.
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

#include <villas/config.h>
#include <villas/hook.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class CapabilitiesRequest : public Request {

public:
	using Request::Request;

	virtual Response * execute()
	{
		json_t *json_hooks = json_array();
		json_t *json_apis  = json_array();
		json_t *json_nodes = json_array();
		json_t *json_formats = json_array();
		json_t *json_name;

		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Capabilities endpoint does not accept any body data");

		for (auto f : plugin::Registry::lookup<RequestFactory>()) {
			json_name = json_string(f->getName().c_str());

			json_array_append_new(json_apis, json_name);
		}

		for (auto f : plugin::Registry::lookup<HookFactory>()) {
			json_name = json_string(f->getName().c_str());

			json_array_append_new(json_hooks, json_name);
		}

		for (size_t i = 0; i < vlist_length(&plugins); i++) {
			struct plugin *p = (struct plugin *) vlist_at(&plugins, i);

			json_t *json_name;
			switch(p->type) {
				case PluginType::NODE:
					json_name = json_string(p->name);
					json_array_append_new(json_nodes, json_name);
					break;

				case PluginType::FORMAT:
					json_name = json_string(p->name);
					json_array_append_new(json_formats, json_name);
					break;
			}
		}

#if 0 /* @todo Port to C++ */
		for (auto f : NodeFactory::lookup()) {
			json_name = json_string(f->getName().c_str());

			json_array_append_new(json_nodes, json_name);
		}

		for (auto f : FormatFactory::lookup()) {
			json_name = json_string(f->getName().c_str());

			json_array_append_new(json_formats, json_name);
		}
#endif

		auto *json_capabilities = json_pack("{ s: o, s: o, s: o, s: o }",
				"hooks", json_hooks,
				"node-types", json_nodes,
				"apis", json_apis,
				"formats", json_formats);

		return new Response(session, json_capabilities);
	}
};

/* Register API request */
static char n[] = "capabilities";
static char r[] = "/capabilities";
static char d[] = "get capabiltities and details about this VILLASnode instance";
static RequestPlugin<CapabilitiesRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
