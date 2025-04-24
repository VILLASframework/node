/* The Prometheus metrics endpoint.
 *
 * Author: Youssef Nakti <youssef.nakti@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
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
      for(auto& metric:metrics_subset){
        Hist histogram = stats->getHistogram(metric.first);
        std::string t =std::to_string(
          std::chrono::duration_cast<std::chrono::milliseconds >(
            std::chrono::system_clock::now().time_since_epoch()
          ).count()
        );
        text_res+=metric.second+" {node=\""+node_name+"\" acc=\"last\"} "+std::to_string(histogram.getLast())+" "+t+"\n";
        text_res+=metric.second+" {node=\""+node_name+"\" acc=\"total\"} "+std::to_string(histogram.getTotal())+" "+t+"\n";
      }
    }

    return new Response(session,HTTP_STATUS_OK, "text/plain; charset=UTF-8", Buffer(text_res.c_str(),text_res.size()));
  }
};

// Register API request
static char n[] = "metrics";
static char r[] = "/metrics";
static char d[] = "Get Prometheus metrics from all nodes";
static RequestPlugin<MetricsRequest, n, r, d> p;

} // namespace api
} // namespace node
} // namespace villas
