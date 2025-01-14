/* WebRTC peer connection.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-FileCopyrightText: 2023 OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fmt/ostream.h>
#include <jansson.h>
#include <rtc/peerconnection.hpp>
#include <rtc/rtc.hpp>
#include <villas/config.hpp>
#include <villas/log.hpp>
#include <villas/node/config.hpp>
#include <villas/nodes/webrtc/signaling_client.hpp>
#include <villas/signal_list.hpp>
#include <villas/web.hpp>

/*
 * libdatachannel defines the operator<< overloads required to format
 * rtc::PeerConnection::State and similar in the global namespace.
 * But C++ ADL based overload set construction does not find these operators,
 * if these are invoked in the spdlog/fmt libraries.
 *
 * See this issue for a short explanation of ADL errors:
 * https://github.com/gabime/spdlog/issues/1227#issuecomment-532009129
 *
 * Adding the global ::operator<< overload set to the namespace rtc where
 * the data structures are defined, allows ADL to pick these up in spdlog/fmt.
 *
 * Since libdatachannel 0.20, operator<< has been moved into the rtc namespace.
 */
#if RTC_VERSION_NUM < 0x001400
namespace rtc {
using ::operator<<;
}
#endif

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<rtc::PeerConnection::State>
    : public fmt::ostream_formatter {};
#endif

namespace villas {
namespace node {
namespace webrtc {

class PeerConnection {

public:
  PeerConnection(const std::string &server, const std::string &session,
                 const std::string &peer,
                 std::shared_ptr<SignalList> out_signals,
                 rtc::Configuration config, Web *w, rtc::DataChannelInit d);
  ~PeerConnection();

  bool waitForDataChannel(std::chrono::seconds timeout);
  void onMessage(std::function<void(rtc::binary)> callback);
  void sendMessage(rtc::binary msg);

  json_t *readStatus() const;

  void connect();
  void disconnect();

protected:
  Web *web;
  std::vector<rtc::IceServer> extraServers;
  rtc::DataChannelInit dataChannelInit;
  rtc::Configuration defaultConfig;

  std::shared_ptr<rtc::PeerConnection> conn;
  std::shared_ptr<rtc::DataChannel> chan;
  std::shared_ptr<SignalingClient> client;
  std::shared_ptr<SignalList> out_signals;

  Logger logger;

  std::mutex mutex;

  std::condition_variable_any startupCondition;
  bool stopStartup;

  bool warnNotConnected;
  bool standby;
  bool first;
  int firstID;
  int secondID;

  std::function<void(rtc::binary)> onMessageCallback;

  void resetConnection(std::unique_lock<decltype(PeerConnection::mutex)> &lock);
  void resetConnectionAndStandby(
      std::unique_lock<decltype(PeerConnection::mutex)> &lock);
  void notifyStartup();

  void setupPeerConnection(std::shared_ptr<rtc::PeerConnection> = nullptr);
  void setupDataChannel(std::shared_ptr<rtc::DataChannel> = nullptr);

  void onLocalDescription(rtc::Description sdp);
  void onLocalCandidate(rtc::Candidate cand);

  void onConnectionStateChange(rtc::PeerConnection::State state);
  void onSignalingStateChange(rtc::PeerConnection::SignalingState state);
  void onGatheringStateChange(rtc::PeerConnection::GatheringState state);

  void onSignalingConnected();
  void onSignalingDisconnected();
  void onSignalingError(std::string err);
  void onSignalingMessage(SignalingMessage msg);

  void onDataChannel(std::shared_ptr<rtc::DataChannel> dc);
  void onDataChannelOpen();
  void onDataChannelClosed();
  void onDataChannelError(std::string err);
  void onDataChannelMessage(rtc::string msg);
  void onDataChannelMessage(rtc::binary msg);
};

} // namespace webrtc
} // namespace node
} // namespace villas
