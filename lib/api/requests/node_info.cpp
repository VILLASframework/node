/* The "node" API ressource.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>

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

class NodeInfoRequest : public NodeRequest {

public:
  using NodeRequest::NodeRequest;

  Response *execute() override {
    if (method != Session::Method::GET)
      throw Error::invalidMethod(this);

    if (body != nullptr)
      throw Error::badRequest(nullptr,
                              "Nodes endpoint does not accept any body data");

    auto *json_node = node->toJson();

    auto sigs = node->getOutputSignals();
    if (sigs) {
      auto *json_out = json_object_get(json_node, "out");
      if (json_out)
        json_object_set_new(json_out, "signals", sigs->toJson());
    }

    return new JsonResponse(session, HTTP_STATUS_OK, json_node);
  }
};

// Register API request
static char n[] = "node";
static char r[] = "/node/(" RE_NODE_NAME "|" RE_UUID ")";
static char d[] = "Get node details";
static RequestPlugin<NodeInfoRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
