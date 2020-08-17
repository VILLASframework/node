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
#include <villas/path_source.h>
#include <villas/path_destination.h>
#include <villas/hook.hpp>
#include <villas/utils.hpp>
#include <villas/api/session.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class PathsRequest : public Request {

public:
	using Request::Request;

	virtual Response * execute()
	{
		if (method != Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Paths endpoint does not accept any body data");

		json_t *json_paths = json_array();

		struct vlist *paths = session->getSuperNode()->getPaths();

		for (size_t i = 0; i < vlist_length(paths); i++) {
			struct vpath *p = (struct vpath *) vlist_at(paths, i);

			char uuid[37];
			uuid_unparse(p->uuid, uuid);

			json_t *json_signals = json_array();
			json_t *json_hooks = json_array();
			json_t *json_sources = json_array();
			json_t *json_destinations = json_array();

			for (size_t i = 0; i < vlist_length(&p->signals); i++) {
				struct signal *sig = (struct signal *) vlist_at_safe(&p->signals, i);

				json_array_append(json_signals, signal_to_json(sig));
			}

			for (size_t i = 0; i < vlist_length(&p->hooks); i++) {
				Hook *h = (Hook *) vlist_at_safe(&p->hooks, i);

				json_array_append(json_hooks, h->getConfig());
			}

			for (size_t i = 0; i < vlist_length(&p->sources); i++) {
				struct vpath_source *pd = (struct vpath_source *) vlist_at_safe(&p->sources, i);

				json_array_append_new(json_sources, json_string(node_name_short(pd->node)));
			}

			for (size_t i = 0; i < vlist_length(&p->destinations); i++) {
				struct vpath_destination *pd = (struct vpath_destination *) vlist_at_safe(&p->destinations, i);

				json_array_append_new(json_destinations, json_string(node_name_short(pd->node)));
			}

			json_t *json_path = json_pack("{ s: s, s: s, s: s, s: b, s: b s: b, s: b, s: b, s: b s: i, s: o, s: o, s: o, s: o }",
				"uuid", uuid,
				"state", state_print(p->state),
				"mode", p->mode == PathMode::ANY ? "any" : "all",
				"enabled", p->enabled,
				"builtin", p->builtin,
				"reverse", p->reverse,
				"original_sequence_no", p->original_sequence_no,
				"last_sequence", p->last_sequence,
				"poll", p->poll,
				"queuelen", p->queuelen,
				"signals", json_signals,
				"hooks", json_hooks,
				"in", json_sources,
				"out", json_destinations
			);

			json_array_append_new(json_paths, json_path);
		}

		return new Response(session, json_paths);
	}
};

/* Register API request */
static char n[] = "paths";
static char r[] = "/paths";
static char d[] = "retrieve list of all paths with details";
static RequestPlugin<PathsRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
