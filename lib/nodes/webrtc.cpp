/* Node-type: webrtc.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <vector>

#include <villas/exceptions.hpp>
#include <villas/node_compat.hpp>
#include <villas/nodes/webrtc.hpp>
#include <villas/sample.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/uuid.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static villas::node::Web *web;

WebRTCNode::WebRTCNode(const uuid_t &id, const std::string &name)
    : Node(id, name),
      server("https://villas.k8s.eonerc.rwth-aachen.de/ws/signaling"),
      peer(uuid::toString(id)), wait_seconds(0), format(nullptr), queue({}),
      pool({}), dci({}) {
  dci.reliability.type = rtc::Reliability::Type::Rexmit;
}

WebRTCNode::~WebRTCNode() {
  int ret = pool_destroy(&pool);
  if (ret) // TODO log
    ;
}

int WebRTCNode::parse(json_t *json) {
  int ret = Node::parse(json);
  if (ret)
    return ret;

  const char *sess;
  const char *svr = nullptr;
  const char *pr = nullptr;
  int ord = -1;
  int &rexmit = dci.reliability.rexmit.emplace<int>(0);
  json_t *json_ice = nullptr;
  json_t *json_format = nullptr;

  json_error_t err;
  ret = json_unpack_ex(
      json, &err, 0, "{ s: s, s?: s, s?: s, s?: i, s?: i, s?: b, s?: o }",
      "session", &sess, "peer", &pr, "server", &svr, "wait_seconds",
      &wait_seconds, "max_retransmits", &rexmit, "ordered", &ord, "ice",
      &json_ice, "format", &json_format);
  if (ret)
    throw ConfigError(json, err, "node-config-node-webrtc");

  session = sess;

  if (svr)
    server = svr;

  if (pr)
    peer = pr;

  if (ord)
    dci.reliability.unordered = !ord;

  if (json_ice) {
    json_t *json_servers = nullptr;

    ret = json_unpack_ex(json_ice, &err, 0, "{ s?: o }", "servers",
                         &json_servers);
    if (ret)
      throw ConfigError(json, err, "node-config-node-webrtc-ice");

    if (json_servers) {
      rtcConf.iceServers.clear();

      if (!json_is_array(json_servers))
        throw ConfigError(
            json_servers, "node-config-node-webrtc-ice-servers",
            "ICE Servers must be a an array of server configurations.");

      size_t i;
      json_t *json_server;
      json_array_foreach(json_servers, i, json_server) {
        if (!json_is_string(json_server))
          throw ConfigError(json_server, "node-config-node-webrtc-ice-server",
                            "ICE servers must be provided as STUN/TURN url.");

        std::string uri = json_string_value(json_server);

        rtcConf.iceServers.emplace_back(uri);
      }
    }
  }

  format = json_format ? FormatFactory::make(json_format)
                       : FormatFactory::make("villas.binary");

  assert(format);

  return 0;
}

int WebRTCNode::prepare() {
  int ret = Node::prepare();
  if (ret)
    return ret;

  format->start(getInputSignals(false), ~(int)SampleFlags::HAS_OFFSET);

  // TODO: Determine output signals reliably
  auto signals = std::make_shared<SignalList>();

  conn = std::make_shared<webrtc::PeerConnection>(server, session, peer,
                                                  signals, rtcConf, web, dci);

  ret = pool_init(&pool, 1024, SAMPLE_LENGTH(getInputSignals(false)->size()));
  if (ret) // TODO log
    return ret;

  ret = queue_signalled_init(&queue, 1024);
  if (ret) // TODO log
    return ret;

  // TODO: Move this to a member function
  conn->onMessage([this](rtc::binary msg) {
    int ret;
    std::vector<Sample *> smps;
    smps.resize(this->in.vectorize);

    ret = sample_alloc_many(&this->pool, smps.data(), smps.size());
    if (ret < 0) // TODO log
      return;

    ret = format->sscan((const char *)msg.data(), msg.size(), nullptr,
                        smps.data(), ret);
    if (ret < 0) // TODO log
      return;

    ret = queue_signalled_push_many(&this->queue, (void **)smps.data(), ret);
    if (ret < 0) // TODO log
      return;

    this->logger->trace(
        "onMessage(rtc::binary) callback finished pushing {} samples", ret);
  });

  return 0;
}

int WebRTCNode::start() {
  int ret = Node::start();
  if (!ret)
    state = State::STARTED;

  conn->connect();

  if (wait_seconds > 0) {
    logger->info("Waiting for datachannel...");

    if (!conn->waitForDataChannel(std::chrono::seconds{wait_seconds})) {
      throw RuntimeError{"Waiting for datachannel timed out after {} seconds",
                         wait_seconds};
    }
  }

  return 0;
}

int WebRTCNode::stop() {
  conn->disconnect();
  return Node::stop();
}

std::vector<int> WebRTCNode::getPollFDs() {
  return {queue_signalled_fd(&queue)};
}

const std::string &WebRTCNode::getDetails() {
  // TODO
  details = fmt::format("");
  return details;
}

int WebRTCNode::_read(struct Sample *smps[], unsigned cnt) {
  std::vector<Sample *> smpt;
  smpt.resize(cnt);

  int pulled =
      queue_signalled_pull_many(&queue, (void **)smpt.data(), smpt.size());

  sample_copy_many(smps, smpt.data(), pulled);
  sample_decref_many(smpt.data(), pulled);

  return pulled;
}

int WebRTCNode::_write(struct Sample *smps[], unsigned cnt) {
  rtc::binary buf;
  size_t wbytes;

  buf.resize(4 * 1024);
  int ret = format->sprint((char *)buf.data(), buf.size(), &wbytes, smps, cnt);
  if (ret < 0) // TODO log
    return ret;

  buf.resize(wbytes);
  conn->sendMessage(buf);

  return ret;
}

json_t *WebRTCNode::_readStatus() const {
  if (!conn)
    return nullptr;

  return conn->readStatus();
}

int WebRTCNodeFactory::start(SuperNode *sn) {
  web = sn->getWeb();
  if (!web->isEnabled())
    return -1;

  return 0;
}

static WebRTCNodeFactory p;
