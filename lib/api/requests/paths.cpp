/* The "paths" API ressource.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>

#include <villas/super_node.hpp>
#include <villas/path.hpp>
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

	virtual
	Response * execute()
	{
		if (method != Session::Method::GET)
			throw InvalidMethod(this);

		if (body != nullptr)
			throw BadRequest("Paths endpoint does not accept any body data");

		json_t *json_paths = session->getSuperNode()->getPaths().toJson();

		return new JsonResponse(session, HTTP_STATUS_OK, json_paths);
	}
};

// Register API request
static char n[] = "paths";
static char r[] = "/paths";
static char d[] = "retrieve list of all paths with details";
static RequestPlugin<PathsRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
