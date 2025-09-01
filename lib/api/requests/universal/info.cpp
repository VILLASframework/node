/* The Universal Data-exchange API.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/requests/universal.hpp>
#include <villas/api/response.hpp>
#include <villas/node.hpp>
#include <villas/uuid.hpp>

namespace villas {
namespace node {
namespace api {
namespace universal {

class InfoRequest : public UniversalRequest {
public:
  using UniversalRequest::UniversalRequest;

  Response *execute() override {
    if (method != Session::Method::GET)
      throw InvalidMethod(this);

    if (body != nullptr)
      throw BadRequest("This endpoint does not accept any body data");

    auto *info = json_pack(
        "{ s: s, s: s, s: { s: s, s: s } }", "id", node->getNameShort().c_str(),
        "uuid", uuid::toString(node->getUuid()).c_str(),

        "transport", "type", "villas", "version", PROJECT_VERSION);

    return new JsonResponse(session, HTTP_STATUS_OK, info);
  }
};

// Register API requests
static char n[] = "universal/info";
static char r[] = "/universal/(" RE_NODE_NAME ")/info";
static char d[] = "Get details of universal data-exchange API";
static RequestPlugin<InfoRequest, n, r, d> p;

} // namespace universal
} // namespace api
} // namespace node
} // namespace villas
