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
	PeerConnection(const std::string &server, const std::string &session, const rtc::Configuration &config, Web *w);
	~PeerConnection();

	void waitForDataChannel();

	void connect();

protected:
	Web *web;
	rtc::Configuration config;

	std::shared_ptr<rtc::PeerConnection> conn;
	std::shared_ptr<rtc::DataChannel> chan;
	std::shared_ptr<SignalingClient> client;

	Logger logger;

	bool makingOffer;
	bool ignoreOffer;
	bool first;
	bool polite;
	bool rollback;

	void createPeerConnection();
	void rollbackPeerConnection();
	void createDatachannel();

	void onConnectionCreated();

	void onLocalDescription(rtc::Description sdp);
	void onLocalCandidate(rtc::Candidate cand);

	void onNegotiationNeeded();

	void onConnectionStateChange(rtc::PeerConnection::State state);
	void onSignalingStateChange(rtc::PeerConnection::SignalingState state);
	void onGatheringStateChange(rtc::PeerConnection::GatheringState state);

	void onSignalingConnected();
	void onSignalingDisconnected();
	void onSignalingError(const std::string &err);
	void onSignalingMessage(const SignalingMessage &msg);

	void onDataChannel(std::shared_ptr<rtc::DataChannel> dc);
	void onDataChannelOpen();
	void onDataChannelClosed();
	void onDataChannelError(std::string err);
	void onDataChannelMessage(rtc::message_variant msg);
};

} /* namespace webrtc */
} /* namespace node */
} /* namespace villas */

