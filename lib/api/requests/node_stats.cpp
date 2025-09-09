/* The API ressource for querying statistics.
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

  Response *execute() override {
    if (method != Session::Method::GET)
      throw Error::invalidMethod(this);

    if (body != nullptr)
      throw Error::badRequest(nullptr,
                              "Stats endpoint does not accept any body data");

    if (node->getStats() == nullptr)
      throw Error::badRequest(
          nullptr, "The statistics collection for this node is not enabled");

    return new JsonResponse(session, HTTP_STATUS_OK,
                            node->getStats()->toJson());
  }
};

// Register API requests
static char n[] = "node/stats";
static char r[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")/stats";
static char d[] = "Get internal statistics counters";
static RequestPlugin<StatsRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
