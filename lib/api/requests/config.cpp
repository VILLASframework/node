/* The "config" API ressource.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/request.hpp>
#include <villas/api/response.hpp>
#include <villas/api/session.hpp>
#include <villas/super_node.hpp>

namespace villas {
namespace node {
namespace api {

class ConfigRequest : public Request {

public:
  using Request::Request;

  Response *execute() override {
    json_t *json = session->getSuperNode()->getConfig();

    if (method != Session::Method::GET)
      throw InvalidMethod(this);

    if (body != nullptr)
      throw BadRequest("Config endpoint does not accept any body data");

    auto *json_config = json ? json_incref(json) : json_object();

    return new JsonResponse(session, HTTP_STATUS_OK, json_config);
  }
};

// Register API request
static char n[] = "config";
static char r[] = "/config";
static char d[] = "Get the configuration of this VILLASnode instance";
static RequestPlugin<ConfigRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
