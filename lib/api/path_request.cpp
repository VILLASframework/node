/** Path API Request.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/api/path_request.hpp>
#include <villas/path.hpp>

using namespace villas::node::api;

void PathRequest::prepare()
{
	int ret;
	uuid_t uuid;

	ret = uuid_parse(matches[1].c_str(), uuid);
	if (ret)
		throw BadRequest("Invalid UUID", "{ s: s }",
			"uuid", matches[1].c_str()
		);

	auto paths = session->getSuperNode()->getPaths();
	path = paths.lookup(uuid);
	if (!path)
		throw BadRequest("No path found with with matching UUID", "{ s: s }",
			"uuid", matches[1].c_str()
		);
}
