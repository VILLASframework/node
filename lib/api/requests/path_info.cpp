/* The "path" API ressource.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>

#include <villas/super_node.hpp>
#include <villas/path.hpp>
#include <villas/utils.hpp>
#include <villas/stats.hpp>
#include <villas/api/session.hpp>
#include <villas/api/requests/path.hpp>
#include <villas/api/response.hpp>

namespace villas {
namespace node {
namespace api {

class PathInfoRequest : public PathRequest {

public:
	using PathRequest::PathRequest;

	virtual
	Response * execute()
	{
		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Endpoint does not accept any body data");

		return new JsonResponse(session, HTTP_STATUS_OK, path->toJson());
	}
};

// Register API request
static char n[] = "path";
static char r[] = "/path/(" RE_UUID ")";
static char d[] = "retrieve info of a path";
static RequestPlugin<PathInfoRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
