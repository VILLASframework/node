/* WebRTC signaling messages.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <algorithm>
#include <fmt/format.h>
#include <villas/exceptions.hpp>
#include <villas/nodes/webrtc/signaling_message.hpp>
#include <villas/utils.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::webrtc;

json_t *Peer::toJson() const {
  return json_pack("{ s: i, s: s*, s: s*, s: s* }", "id", id, "name",
                   name.empty() ? nullptr : name.c_str(), "remote",
                   remote.empty() ? nullptr : remote.c_str(), "user_agent",
                   userAgent.empty() ? nullptr : userAgent.c_str()
                   // TODO: created, connected
  );
}

Peer::Peer(json_t *json) {
  const char *nme = nullptr, *rem = nullptr, *ua = nullptr, *tscreat, *tsconn;

  int ret = json_unpack(json, "{ s: i, s?: s, s?: s, s?: s, s?: s, s?: s }",
                        "id", &id, "name", &nme, "remote", &rem, "user_agent",
                        &ua, "created", &tscreat, "connected", &tsconn);
  if (ret)
    throw RuntimeError("Failed to decode signaling message");

  if (nme)
    name = nme;

  if (rem)
    remote = rem;

  if (ua)
    userAgent = ua;

  // TODO: created, connected
}

RelayMessage::RelayMessage(json_t *json) {

  if (!json_is_array(json))
    throw RuntimeError("Failed to decode signaling message");

  int ret;
  char *url;
  char *user;
  char *pass;
  char *realm;
  char *expires;
  json_t *json_server;
  size_t i;
  json_array_foreach(json, i, json_server) {
    ret = json_unpack(json_server, "{ s: s, s: s, s: s, s: s, s: s }", "url",
                      &url, "user", &user, "pass", &pass, "realm", &realm,
                      "expires", &expires);
    if (ret)
      throw RuntimeError("Failed to decode signaling message");

    auto &server = servers.emplace_back(url);
    server.username = user;
    server.password = pass;

    // TODO: warn about unsupported realm
    // TODO: log info about expires time
  }
}

json_t *ControlMessage::toJson() const {
  json_t *json_peers = json_array();

  for (auto &p : peers) {
    json_t *json_peer = p.toJson();

    json_array_append_new(json_peers, json_peer);
  }

  return json_pack("{ s: i, s: o }", "peer_id", peerID, "peers", json_peers);
}

ControlMessage::ControlMessage(json_t *j) {
  int ret;

  json_t *json_peers;

  ret = json_unpack(j, "{ s: i, s: o }", "peer_id", &peerID, "peers",
                    &json_peers);
  if (ret)
    throw RuntimeError("Failed to decode signaling message");

  if (!json_is_array(json_peers))
    throw RuntimeError("Failed to decode signaling message");

  json_t *json_peer;
  size_t i;
  // cppcheck-suppress unknownMacro
  json_array_foreach(json_peers, i, json_peer) peers.emplace_back(json_peer);
}

json_t *SignalingMessage::toJson() const {
  return std::visit(
      villas::utils::overloaded{
          [](ControlMessage const &c) {
            return json_pack("{ s: o }", "control", c.toJson());
          },
          [](SignalList const &s) {
            return json_pack("{ s: o }", "signals", s.toJson());
          },
          [](rtc::Description const &d) {
            return json_pack("{ s: { s: s, s: s } }", "description", "spd",
                             d.generateSdp().c_str(), "type",
                             d.typeString().c_str());
          },
          [](rtc::Candidate const &c) {
            return json_pack("{ s: { s: s, s: s } }", "candidate", "spd",
                             c.candidate().c_str(), "mid", c.mid().c_str());
          },
          [](auto &other) { return (json_t *){nullptr}; }},
      message);
}

std::string SignalingMessage::toString() const {
  return std::visit(
      villas::utils::overloaded{
          [](RelayMessage const &r) { return fmt::format("type=relay"); },
          [](ControlMessage const &c) {
            return fmt::format("type=control, control={}",
                               json_dumps(c.toJson(), 0));
          },
          [](SignalList const &s) { return fmt::format("type=signal"); },
          [](rtc::Description const &d) {
            return fmt::format("type=description, type={}, spd=\n{}",
                               d.typeString(), d.generateSdp());
          },
          [](rtc::Candidate const &c) {
            return fmt::format("type=candidate, mid={}, spd=\n{}",
                               c.candidate(), c.mid());
          },
          [](auto other) { return fmt::format("<invalid>"); }},
      message);
}

SignalingMessage SignalingMessage::fromJson(json_t *json) {
  auto self = SignalingMessage{std::monostate()};

  // Relay message
  json_t *rlys = nullptr;
  // Signal message
  json_t *sigs = nullptr;
  // Control message
  json_t *ctrl = nullptr;
  // Candidate message
  const char *cand = nullptr;
  const char *mid = nullptr;
  // Description message
  const char *desc = nullptr;
  const char *typ = nullptr;

  int ret = json_unpack(
      json, "{ s?: o, s?: o, s?: o, s?: { s: s, s: s }, s?: { s: s, s: s } }",
      "servers", &rlys, "signals", &sigs, "control", &ctrl, "candidate", "spd",
      &cand, "mid", &mid, "description", "spd", &desc, "type", &typ);

  // Exactly 1 field may be specified
  const void *fields[] = {ctrl, cand, desc};
  if (ret || std::count(std::begin(fields), std::end(fields), nullptr) <
                 std::make_signed_t<size_t>(std::size(fields)) - 1)
    throw RuntimeError("Failed to decode signaling message");

  if (rlys) {
    self.message.emplace<RelayMessage>(rlys);
  } else if (sigs) {
    self.message.emplace<SignalList>(sigs);
  } else if (ctrl) {
    self.message.emplace<ControlMessage>(ctrl);
  } else if (cand) {
    self.message.emplace<rtc::Candidate>(cand, mid);
  } else if (desc) {
    self.message.emplace<rtc::Description>(desc, typ);
  }

  return self;
}
