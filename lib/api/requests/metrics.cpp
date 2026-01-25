/* The metrics API ressource.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Youssef Nakti <naktiyoussef@proton.me>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>

#include <jansson.h>

#include <villas/api/request.hpp>
#include <villas/api/response.hpp>
#include <villas/api/session.hpp>
#include <villas/node.hpp>
#include <villas/stats.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>

namespace villas {
namespace node {
namespace api {

class MetricsRequest : public Request {
public:
  using Request::Request;

  Response *execute() override {
    if (method != Session::Method::GET) {
      throw Error::invalidMethod(this);
    }

    if (body != nullptr) {
      throw Error::badRequest(
          nullptr, "The metrics endpoint does not accept any body data");
    }

    std::stringstream ss;
    NodeList node_list = session->getSuperNode()->getNodes();

    for (Node *node : node_list) {
      auto stats = node->getStats();
      if (!stats)
        continue;

      std::string node_name = node->getNameShort();
      for (auto &metric : Stats::metrics) {
        std::string metric_name = metric.second.name;
        std::replace(metric_name.begin(), metric_name.end(), '.', '_');
        ss << stats->getHistogram(metric.first)
                  .toPrometheusText(metric_name, node->getNameShort())
           << "\n\n";
      }
    }

    auto str = ss.str();
    return new Response(session, HTTP_STATUS_OK, "text/plain; charset=UTF-8",
                        Buffer(str.c_str(), str.size()));
  }
};

// Register API request
static char n[] = "metrics";
static char r[] = "/metrics";
static char d[] = "Get stats of all nodes in Prometheus metrics format";
static RequestPlugin<MetricsRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
