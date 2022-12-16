/** Path API Request.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/api/requests/path.hpp>
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
