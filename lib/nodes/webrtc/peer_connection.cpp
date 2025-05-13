/* WebRTC peer connection.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <chrono>
#include <functional>

#include <fmt/chrono.h>
#include <fmt/core.h>
#include <fmt/ostream.h>
#include <rtc/common.hpp>

#include <villas/exceptions.hpp>
#include <villas/nodes/webrtc/peer_connection.hpp>
#include <villas/utils.hpp>

using namespace std::placeholders;

using namespace villas;
using namespace villas::node;
using namespace villas::node::webrtc;

PeerConnection::PeerConnection(const std::string &server,
                               const std::string &session,
                               const std::string &peer,
                               std::shared_ptr<SignalList> out_signals,
                               rtc::Configuration cfg, Web *w,
                               rtc::DataChannelInit d)
    : web(w), extraServers({}), dataChannelInit(d), defaultConfig(cfg),
      conn(nullptr), chan(nullptr), out_signals(out_signals),
      logger(Log::get("webrtc:pc")), stopStartup(false),
      warnNotConnected(false), standby(true), first(false), firstID(INT_MAX),
      secondID(INT_MAX), onMessageCallback(nullptr) {
  client = std::make_shared<SignalingClient>(server, session, peer, web);

  client->onConnected(std::bind(&PeerConnection::onSignalingConnected, this));
  client->onDisconnected(
      std::bind(&PeerConnection::onSignalingDisconnected, this));
  client->onError(std::bind(&PeerConnection::onSignalingError, this,
                            std::placeholders::_1));
  client->onMessage(std::bind(&PeerConnection::onSignalingMessage, this,
                              std::placeholders::_1));

  auto lock = std::unique_lock{mutex};
  resetConnectionAndStandby(lock);
}

PeerConnection::~PeerConnection() {}

bool PeerConnection::waitForDataChannel(std::chrono::seconds timeout) {
  auto lock = std::unique_lock{mutex};

  auto deadline = std::chrono::steady_clock::now() + timeout;

  return startupCondition.wait_until(lock, deadline,
                                     [this]() { return this->stopStartup; });
}

json_t *PeerConnection::readStatus() const {
  auto *json =
      json_pack("{ s: I, s: I }", "bytes_received", conn->bytesReceived(),
                "bytes_sent", conn->bytesSent());

  auto rtt = conn->rtt();
  if (rtt.has_value()) {
    auto *json_rtt = json_real(rtt.value().count() / 1e3);
    json_object_set_new(json, "rtt", json_rtt);
  }

  rtc::Candidate local, remote;
  if (conn->getSelectedCandidatePair(&local, &remote)) {
    auto *json_cp =
        json_pack("{ s: s, s: s }", "local", std::string(local).c_str(),
                  "remote", std::string(remote).c_str());
    json_object_set_new(json, "candidate_pair", json_cp);
  }

  return json;
}

void PeerConnection::notifyStartup() {
  stopStartup = true;
  startupCondition.notify_all();
}

void PeerConnection::onMessage(std::function<void(rtc::binary)> callback) {
  auto lock = std::unique_lock{mutex};

  onMessageCallback = callback;
}

void PeerConnection::sendMessage(rtc::binary msg) {
  auto lock = std::unique_lock{mutex};

  if (chan && chan->isOpen()) {
    chan->send(msg);
    warnNotConnected = true;
  } else if (warnNotConnected) {
    logger->warn("Dropping messages. No peer connected.");
    warnNotConnected = false;
  }
}

void PeerConnection::connect() { client->connect(); }

void PeerConnection::disconnect() { client->disconnect(); }

void PeerConnection::resetConnection(
    std::unique_lock<decltype(PeerConnection::mutex)> &lock) {
  lock.unlock();
  chan.reset();
  conn.reset();
  lock.lock();
}

void PeerConnection::resetConnectionAndStandby(
    std::unique_lock<decltype(PeerConnection::mutex)> &lock) {
  if (!standby)
    logger->info("Going to standby");

  standby = true;
  first = false;
  firstID = INT_MAX;
  secondID = INT_MAX;
  warnNotConnected = false;

  resetConnection(lock);
}

void PeerConnection::setupPeerConnection(
    std::shared_ptr<rtc::PeerConnection> pc) {
  logger->debug("Setup {} peer connection", pc ? "existing" : "new");

  auto config = defaultConfig;
  config.iceServers.insert(std::end(config.iceServers),
                           std::begin(extraServers), std::end(extraServers));

  conn = pc ? std::move(pc) : std::make_shared<rtc::PeerConnection>(config);
  conn->onLocalDescription(std::bind(&PeerConnection::onLocalDescription, this,
                                     std::placeholders::_1));
  conn->onLocalCandidate(std::bind(&PeerConnection::onLocalCandidate, this,
                                   std::placeholders::_1));
  conn->onDataChannel(
      std::bind(&PeerConnection::onDataChannel, this, std::placeholders::_1));
  conn->onGatheringStateChange(std::bind(
      &PeerConnection::onGatheringStateChange, this, std::placeholders::_1));
  conn->onSignalingStateChange(std::bind(
      &PeerConnection::onSignalingStateChange, this, std::placeholders::_1));
  conn->onStateChange(std::bind(&PeerConnection::onConnectionStateChange, this,
                                std::placeholders::_1));
}

void PeerConnection::setupDataChannel(std::shared_ptr<rtc::DataChannel> dc) {
  logger->debug("Setup {} data channel", dc ? "existing" : "new");

  assert(conn);
  chan =
      dc ? std::move(dc) : conn->createDataChannel("villas", dataChannelInit);

  chan->onMessage(
      [this](rtc::binary msg) { this->onDataChannelMessage(std::move(msg)); },
      [this](rtc::string msg) { this->onDataChannelMessage(std::move(msg)); });
  chan->onOpen(std::bind(&PeerConnection::onDataChannelOpen, this));
  chan->onClosed(std::bind(&PeerConnection::onDataChannelClosed, this));
  chan->onError(std::bind(&PeerConnection::onDataChannelError, this,
                          std::placeholders::_1));

  // If this node has it's data channel set up, don't accept any new ones
  conn->onDataChannel(nullptr);
}

void PeerConnection::onLocalDescription(rtc::Description desc) {
  logger->debug("New local description: type={} sdp=\n{}", desc.typeString(),
                desc.generateSdp());

  auto lock = std::unique_lock{mutex};

  client->sendMessage({desc});
}

void PeerConnection::onLocalCandidate(rtc::Candidate cand) {
  logger->debug("New local candidate: {}", std::string{cand});

  auto lock = std::unique_lock{mutex};

  client->sendMessage({cand});
}

void PeerConnection::onConnectionStateChange(rtc::PeerConnection::State state) {
  logger->debug("Connection State changed: {}", state);

  auto lock = std::unique_lock{mutex};

  switch (state) {
  case rtc::PeerConnection::State::New: {
    logger->debug("New peer connection");
    break;
  }

  case rtc::PeerConnection::State::Connecting: {
    logger->debug("Peer connection connecting");
    break;
  }

  case rtc::PeerConnection::State::Connected: {
    rtc::Candidate local, remote;
    std::optional<std::chrono::milliseconds> rtt = conn->rtt();
    if (conn->getSelectedCandidatePair(&local, &remote)) {
      std::stringstream l, r;
      l << local, r << remote;
      logger->info("Peer connection connected: local={}, remote={}, rtt={}",
                   l.str(), r.str(),
                   rtt.value_or(decltype(rtt)::value_type{0}));
    } else {
      logger->warn("Peer connection connected.\n"
                   "Could not get candidate pair info.\n");
    }
    break;
  }

  case rtc::PeerConnection::State::Disconnected:
  case rtc::PeerConnection::State::Failed: {
    logger->debug("Closing peer connection");
    break;
  }

  case rtc::PeerConnection::State::Closed: {
    logger->debug("Closed peer connection");
    resetConnectionAndStandby(lock);
    break;
  }
  }
}

void PeerConnection::onSignalingStateChange(
    rtc::PeerConnection::SignalingState state) {
  std::stringstream s;
  s << state;
  logger->debug("Signaling state changed: {}", s.str());
}

void PeerConnection::onGatheringStateChange(
    rtc::PeerConnection::GatheringState state) {
  std::stringstream s;
  s << state;
  logger->debug("Gathering state changed: {}", s.str());
}

void PeerConnection::onSignalingConnected() {
  logger->debug("Signaling connection established");

  auto lock = std::unique_lock{mutex};

  client->sendMessage({*out_signals});
}

void PeerConnection::onSignalingDisconnected() {
  logger->debug("Signaling connection closed");

  auto lock = std::unique_lock{mutex};

  resetConnectionAndStandby(lock);
}

void PeerConnection::onSignalingError(std::string err) {
  logger->error("Signaling connection error: {}", err);

  auto lock = std::unique_lock{mutex};

  resetConnectionAndStandby(lock);
}

void PeerConnection::onSignalingMessage(SignalingMessage msg) {
  logger->debug("Signaling message received: {}", msg.toString());

  auto lock = std::unique_lock{mutex};

  std::visit(
      villas::utils::overloaded{
          [&](RelayMessage &c) {
            for (auto &server : c.servers) {
              logger->debug("Received ICE server: {}", server.hostname);
            }

            extraServers = std::move(c.servers);
          },

          [&](ControlMessage &c) {
            auto const &id = c.peerID;

            if (c.peers.size() < 2) {
              logger->warn("Not enough peers connected. Waiting for more.");
              resetConnectionAndStandby(lock);
              return;
            }

            auto fst = INT_MAX, snd = INT_MAX;
            for (auto &c : c.peers) {
              if (c.id < fst) {
                snd = fst;
                fst = c.id;
              } else if (c.id < snd) {
                snd = c.id;
              }
            }

            standby = (id != fst && id != snd);

            if (standby) {
              logger->error("There are already two peers connected to this "
                            "session. Waiting in standby.");
              return;
            }

            if (fst == firstID && snd == secondID) {
              logger->debug("Ignoring control message. This connection is "
                            "already being established.");
              return;
            }

            resetConnection(lock);

            first = (id == fst);
            firstID = fst;
            secondID = snd;

            setupPeerConnection();

            if (!first) {
              setupDataChannel();
              conn->setLocalDescription(rtc::Description::Type::Offer);
            }

            logger->trace("New connection pair: first={}, second={}, I am {}",
                          firstID, secondID, first ? "first" : "second");
          },

          [&](rtc::Description d) {
            if (standby || !conn ||
                (!first && d.type() == rtc::Description::Type::Offer))
              return;

            conn->setRemoteDescription(d);
          },

          [&](rtc::Candidate c) {
            if (standby || !conn)
              return;

            conn->addRemoteCandidate(c);
          },

          [&](auto other) {
            logger->debug("Signaling message has been skipped");
          }},
      msg.message);
}

void PeerConnection::onDataChannel(std::shared_ptr<rtc::DataChannel> dc) {
  logger->debug(
      "New data channel: {}protocol={}, max_msg_size={}, label={}",
      dc->id() && dc->stream()
          ? fmt::format("id={}, stream={}, ", *(dc->id()), *(dc->stream()))
          : "",
      dc->protocol(), dc->maxMessageSize(), dc->label());

  auto lock = std::unique_lock{mutex};

  setupDataChannel(std::move(dc));
}

void PeerConnection::onDataChannelOpen() {
  logger->debug("Datachannel opened");

  auto lock = std::unique_lock{mutex};

  chan->send("Hello from VILLASnode");

  notifyStartup();
}

void PeerConnection::onDataChannelClosed() {
  logger->debug("Datachannel closed");

  auto lock = std::unique_lock{mutex};

  resetConnectionAndStandby(lock);
}

void PeerConnection::onDataChannelError(std::string err) {
  logger->error("Datachannel error: {}", err);

  auto lock = std::unique_lock{mutex};

  resetConnectionAndStandby(lock);
}

void PeerConnection::onDataChannelMessage(rtc::string msg) {
  logger->trace("Received: {}", msg);
}

void PeerConnection::onDataChannelMessage(rtc::binary msg) {
  logger->trace("Received binary data");

  auto lock = std::unique_lock{mutex};

  if (onMessageCallback)
    onMessageCallback(msg);
}
