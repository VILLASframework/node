/* Node API Request.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/requests/node.hpp>

using namespace villas::node::api;

void NodeRequest::prepare() {
  int ret;

  auto &nodes = session->getSuperNode()->getNodes();

  uuid_t uuid;
  ret = uuid_parse(matches[1].c_str(), uuid);
  if (ret) {
    node = nodes.lookup(matches[1]);
    if (!node)
      throw BadRequest("Unknown node", "{ s: s }", "node", matches[1].c_str());
  } else {
    node = nodes.lookup(uuid);
    if (!node)
      throw BadRequest("No node found with with matching UUID", "{ s: s }",
                       "uuid", matches[1].c_str());
  }
}
