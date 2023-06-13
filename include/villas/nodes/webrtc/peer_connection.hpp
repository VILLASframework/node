/** WebRTC peer connection
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <rtc/rtc.hpp>

#include <villas/log.hpp>
#include <villas/web.hpp>
#include <villas/nodes/webrtc/signaling_client.hpp>

namespace villas {
namespace node {
namespace webrtc {

class PeerConnection {

public:
	PeerConnection(const std::string &server, const std::string &session, rtc::Configuration config, Web *w, rtc::DataChannelInit d);
	~PeerConnection();

	bool waitForDataChannel(std::chrono::seconds timeout);
	void onMessage(std::function<void(rtc::binary)> callback);
	void sendMessage(rtc::binary msg);

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
	void resetConnectionAndStandby(std::unique_lock<decltype(PeerConnection::mutex)> &lock);
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

} /* namespace webrtc */
} /* namespace node */
} /* namespace villas */

