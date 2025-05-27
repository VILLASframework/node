/* Node-type webrtc.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <rtc/rtc.hpp>

#include <villas/format.hpp>
#include <villas/node.hpp>
#include <villas/node/config.hpp>
#include <villas/nodes/webrtc/peer_connection.hpp>
#include <villas/queue_signalled.h>
#include <villas/timing.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;

class WebRTCNode : public Node {

protected:
  std::string server;
  std::string session;
  std::string peer;

  int wait_seconds;
  Format::Ptr formatter;
  struct CQueueSignalled queue;
  struct Pool pool;

  std::shared_ptr<webrtc::PeerConnection> conn;
  rtc::Configuration rtcConf;
  rtc::DataChannelInit dci;

  void onMessage(rtc::binary msg);

  int _read(struct Sample *smps[], unsigned cnt) override;
  int _write(struct Sample *smps[], unsigned cnt) override;

  json_t *_readStatus() const override;

public:
  WebRTCNode(const uuid_t &id = {}, const std::string &name = "");

  ~WebRTCNode() override;

  int prepare() override;

  int parse(json_t *json) override;

  int start() override;

  int stop() override;

  std::vector<int> getPollFDs() override;

  const std::string &getDetails() override;
};

class WebRTCNodeFactory : public NodeFactory {

public:
  using NodeFactory::NodeFactory;

  Node *make(const uuid_t &id = {}, const std::string &nme = "") override {
    auto *n = new WebRTCNode(id, nme);

    init(n);

    return n;
  }

  int getFlags() const override {
    return (int)NodeFactory::Flags::SUPPORTS_READ |
           (int)NodeFactory::Flags::SUPPORTS_WRITE |
           (int)NodeFactory::Flags::SUPPORTS_POLL |
           (int)NodeFactory::Flags::REQUIRES_WEB;
  }

  std::string getName() const override { return "webrtc"; }

  std::string getDescription() const override {
    return "Web Real-time Communication";
  }

  int start(SuperNode *sn) override;
};

} // namespace node
} // namespace villas
