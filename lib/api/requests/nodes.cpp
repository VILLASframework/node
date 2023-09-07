/* The "nodes" API ressource.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>

#include <villas/api/request.hpp>
#include <villas/api/response.hpp>
#include <villas/api/session.hpp>
#include <villas/node.hpp>
#include <villas/stats.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

namespace villas {
namespace node {
namespace api {

class NodesRequest : public Request {

public:
  using Request::Request;

  virtual Response *execute() {
    if (method != Session::Method::GET)
      throw InvalidMethod(this);

    if (body != nullptr)
      throw BadRequest("Nodes endpoint does not accept any body data");

    json_t *json_nodes = session->getSuperNode()->getNodes().toJson();

    return new JsonResponse(session, HTTP_STATUS_OK, json_nodes);
  }
};

// Register API request
static char n[] = "nodes";
static char r[] = "/nodes";
static char d[] = "retrieve list of all known nodes";
static RequestPlugin<NodesRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
