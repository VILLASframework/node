/* Path API Request.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/requests/path.hpp>
#include <villas/path.hpp>

using namespace villas::node::api;

void PathRequest::prepare() {
  int ret;
  uuid_t uuid;

  ret = uuid_parse(matches[1].c_str(), uuid);
  if (ret)
    throw Error::badRequest(json_pack("{ s: s }", "uuid", matches[1].c_str()),
                            "Invalid UUID");

  auto paths = session->getSuperNode()->getPaths();
  path = paths.lookup(uuid);
  if (!path)
    throw Error::badRequest(json_pack("{ s: s }", "uuid", matches[1].c_str()),
                            "No path found with with matching UUID");
}
