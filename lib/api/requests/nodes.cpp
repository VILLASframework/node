/** The "nodes" API ressource.
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
#include <uuid/uuid.h>

#include <villas/super_node.hpp>
#include <villas/node.h>
#include <villas/utils.hpp>
#include <villas/stats.hpp>
#include <villas/api/session.hpp>
#include <villas/api/request.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class NodesRequest : public Request {

public:
	using Request::Request;

	virtual Response * execute()
	{
		if (method != Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Nodes endpoint does not accept any body data");

		json_t *json_nodes = json_array();

		struct vlist *nodes = session->getSuperNode()->getNodes();

		for (size_t i = 0; i < vlist_length(nodes); i++) {
			struct node *n = (struct node *) vlist_at(nodes, i);

			struct vlist *output_signals;

			json_t *json_node;
			json_t *json_signals_in = nullptr;
			json_t *json_signals_out = nullptr;

			char uuid[37];
			uuid_unparse(n->uuid, uuid);

			json_signals_in = signal_list_to_json(&n->in.signals);

			output_signals = node_output_signals(n);
			if (output_signals)
				json_signals_out = signal_list_to_json(output_signals);

			json_node = json_pack("{ s: s, s: s, s: s, s: i, s: { s: i, s: o? }, s: { s: i, s: o? } }",
				"name",		node_name_short(n),
				"uuid", 	uuid,
				"state",	state_print(n->state),
				"affinity",	n->affinity,
				"in",
					"vectorize",	n->in.vectorize,
					"signals", 	json_signals_in,
				"out",
					"vectorize",	n->out.vectorize,
					"signals",	json_signals_out
			);

			if (n->stats)
				json_object_set_new(json_node, "stats", n->stats->toJson());

			/* Add all additional fields of node here.
			 * This can be used for metadata */
			json_object_update(json_node, n->cfg);

			json_array_append_new(json_nodes, json_node);
		}

		return new Response(session, json_nodes);
	}
};

/* Register API request */
static char n[] = "nodes";
static char r[] = "/nodes";
static char d[] = "retrieve list of all known nodes";
static RequestPlugin<NodesRequest, n, r, d> p;

} /* namespace api */
} /* namespace node */
} /* namespace villas */
