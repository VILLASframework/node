/* Universal Data-exchange API request.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/api/requests/node.hpp>
#include <villas/nodes/gateway.hpp>

namespace villas {
namespace node {

// Forward declarations
class Node;

namespace api {

class GatewayRequest : public NodeRequest {

protected:
  GatewayNode *gateway_node;

public:
  using NodeRequest::NodeRequest;

  void prepare() override;
};

} // namespace api
} // namespace node
} // namespace villas
