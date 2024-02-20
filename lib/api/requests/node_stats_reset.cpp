/* The API ressource for resetting statistics.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>

#include <villas/api.hpp>
#include <villas/api/requests/node.hpp>
#include <villas/api/response.hpp>
#include <villas/api/session.hpp>
#include <villas/node.hpp>
#include <villas/stats.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

namespace villas {
namespace node {
namespace api {

class StatsRequest : public NodeRequest {

public:
  using NodeRequest::NodeRequest;

  virtual Response *execute() {
    if (method != Session::Method::POST)
      throw InvalidMethod(this);

    if (body != nullptr)
      throw BadRequest("Stats endpoint does not accept any body data");

    if (node->getStats() == nullptr)
      throw BadRequest(
          "The statistics collection for this node is not enabled");

    node->getStats()->reset();

    return new Response(session, HTTP_STATUS_OK);
  }
};

// Register API requests
static char n[] = "node/stats/reset";
static char r[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/stats/reset";
static char d[] = "reset internal statistics counters";
static RequestPlugin<StatsRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
