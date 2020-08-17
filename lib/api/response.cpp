/** API Response.
 *
 * @file
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

#include <villas/api/response.hpp>
#include <villas/api/request.hpp>

using namespace villas::node::api;

Response::Response(Session *s, json_t *resp) :
	session(s),
	response(resp),
	code(HTTP_STATUS_OK)
{
	logger = logging.get("api:response");
}

Response::~Response()
{
	if (response)
		json_decref(response);
}

json_t * ErrorResponse::toJson()
{
	return json_pack("{ s: s }",
		"error", error.c_str()
	);
}
