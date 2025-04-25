/* The "nodes" API ressource.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <jansson.h>
#include<chrono>
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
private:
std::unordered_map<Stats::Metric,std::string> metrics_subset = {
    {Stats::Metric::SMPS_SKIPPED,"skipped"},
    {Stats::Metric::OWD,"owd"},
    {Stats::Metric::AGE,"age"},
    {Stats::Metric::SIGNAL_COUNT,"signalcnt"},
    {Stats::Metric::RTP_PKTS_LOST,"rtp_pkts_lost"}
};

public:
  using Request::Request;

  virtual Response *execute() {
    if (method != Session::Method::GET)
      throw InvalidMethod(this);

    if (body != nullptr)
      throw BadRequest("Nodes endpoint does not accept any body data");
      
    std::string text_res = "";
    NodeList node_list = session->getSuperNode()->getNodes();
    for(Node* node: node_list){
      auto stats = node->getStats();
      if(!stats)
        continue;
      std::string node_name = node->getNameShort();
      for(auto& metric:Stats::metrics){
        std::string metric_name = metric.second.name;
        std::replace(metric_name.begin(),metric_name.end(),'.','_');
        text_res+= stats->getHistogram(metric.first).promFormat(metric_name,node->getNameShort())+"\n";
      }
    }

    return new Response(session,HTTP_STATUS_OK, "text/plain; charset=UTF-8", Buffer(text_res.c_str(),text_res.size()));
  }
};

// Register API request
static char n[] = "metrics";
static char r[] = "/metrics";
static char d[] = "Get stats of all nodes in desired format";
static RequestPlugin<MetricsRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
