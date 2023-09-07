/* The Universal Data-exchange API.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/api/requests/universal.hpp>
#include <villas/api/response.hpp>
#include <villas/utils.hpp>

namespace villas {
namespace node {
namespace api {
namespace universal {

class SignalsRequest : public UniversalRequest {
public:
  using UniversalRequest::UniversalRequest;

  virtual Response *execute() {
    if (method != Session::Method::GET)
      throw InvalidMethod(this);

    if (body != nullptr)
      throw BadRequest("This endpoint does not accept any body data");

    auto *json_sigs = json_array();

    for (size_t i = 0; i < MIN(api_node->getOutputSignals()->size(),
                               api_node->write.channels.size());
         i++) {
      auto sig = api_node->getOutputSignals()->at(i);
      auto ch = api_node->write.channels.at(i);
      auto *json_sig = ch->toJson(sig);

      json_array_append(json_sigs, json_sig);
    }

    for (size_t i = 0; i < MIN(api_node->getInputSignals()->size(),
                               api_node->read.channels.size());
         i++) {
      auto sig = api_node->getInputSignals()->at(i);
      auto ch = api_node->read.channels.at(i);
      auto *json_sig = ch->toJson(sig);

      json_array_append(json_sigs, json_sig);
    }

    return new JsonResponse(session, HTTP_STATUS_OK, json_sigs);
  }
};

// Register API requests
static char n[] = "universal/channels";
static char r[] = "/universal/(" RE_NODE_NAME ")/channels";
static char d[] = "get channels of universal data-exchange API node";
static RequestPlugin<SignalsRequest, n, r, d> p;

} // namespace universal
} // namespace api
} // namespace node
} // namespace villas
