/* WebRTC signaling messages.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <string>
#include <variant>
#include <vector>

#include <jansson.h>
#include <rtc/rtc.hpp>

#include <villas/signal_list.hpp>

namespace villas {
namespace node {
namespace webrtc {

struct Peer {
  int id;
  std::string name;
  std::string remote;
  std::string userAgent;
  std::chrono::time_point<std::chrono::system_clock> created;

  Peer(json_t *j);
  json_t *toJson() const;
};

struct RelayMessage {
  std::vector<rtc::IceServer> servers;

  RelayMessage(json_t *j);
};

struct ControlMessage {
  int peerID;
  std::vector<Peer> peers;

  ControlMessage(json_t *j);
  json_t *toJson() const;
};

struct SignalingMessage {
  std::variant<std::monostate, RelayMessage, ControlMessage, SignalList,
               rtc::Description, rtc::Candidate>
      message;

  static SignalingMessage fromJson(json_t *j);
  json_t *toJson() const;
  std::string toString() const;
};

} // namespace webrtc
} // namespace node
} // namespace villas
