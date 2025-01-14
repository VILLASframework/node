/* API Request for nodes.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/api/request.hpp>

namespace villas {
namespace node {

// Forward declarations
class Node;

namespace api {

class NodeRequest : public Request {

protected:
  Node *node;

public:
  using Request::Request;

  virtual void prepare();
};

} // namespace api
} // namespace node
} // namespace villas
