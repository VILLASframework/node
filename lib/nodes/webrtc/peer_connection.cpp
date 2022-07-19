/** WebRTC peer connection
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <chrono>
#include <thread>

#include <villas/nodes/webrtc/peer_connection.hpp>

using namespace std::placeholders;

using namespace villas;
using namespace villas::node;
using namespace villas::node::webrtc;

PeerConnection::PeerConnection(const std::string &server, const std::string &session, const rtc::Configuration &cfg, Web *w) :
	web(w),
	config(cfg),
	logger(logging.get("webrtc:pc"))

{
	client = std::make_shared<SignalingClient>(server, session, web);

	client->onConnected(std::bind(&PeerConnection::onSignalingConnected, this));
	client->onDisconnected(std::bind(&PeerConnection::onSignalingDisconnected, this));
	client->onError(std::bind(&PeerConnection::onSignalingError, this, _1));
	client->onMessage(std::bind(&PeerConnection::onSignalingMessage, this, _1));
}

PeerConnection::~PeerConnection()
{
	// TODO
}

void PeerConnection::connect()
{
	client->connect();
}

void PeerConnection::waitForDataChannel()
{
	while (!chan) {
		// TODO: signal available via condition variable
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void PeerConnection::createPeerConnection()
{
	// Setup peer connection
	conn = std::make_shared<rtc::PeerConnection>(config);

	conn->onLocalDescription(std::bind(&PeerConnection::onLocalDescription, this, _1));
	conn->onLocalCandidate(std::bind(&PeerConnection::onLocalCandidate, this, _1));

	conn->onDataChannel(std::bind(&PeerConnection::onDataChannel, this, _1));
	conn->onGatheringStateChange(std::bind(&PeerConnection::onGatheringStateChange, this, _1));
	conn->onSignalingStateChange(std::bind(&PeerConnection::onSignalingStateChange, this, _1));
	conn->onStateChange(std::bind(&PeerConnection::onConnectionStateChange, this, _1));
}

void PeerConnection::rollbackPeerConnection()
{

}

void PeerConnection::createDatachannel()
{

}

void PeerConnection::onConnectionCreated()
{
	logger->debug("Connection created");
}

void PeerConnection::onLocalDescription(rtc::Description sdp)
{
	logger->debug("New local description: {}", std::string(sdp));

	SignalingMessage msg(sdp);

	client->sendMessage(msg);
}

void PeerConnection::onLocalCandidate(rtc::Candidate cand)
{
	logger->debug("New local candidate: {}", std::string(cand));

	SignalingMessage msg(cand);

	client->sendMessage(msg);
}

void PeerConnection::onNegotiationNeeded()
{
	logger->debug("Negotation needed");
}

void PeerConnection::onConnectionStateChange(rtc::PeerConnection::State state)
{
	logger->debug("Connection State changed: {}", state);
}

void PeerConnection::onSignalingStateChange(rtc::PeerConnection::SignalingState state)
{
	logger->debug("Signaling state changed: {}", state);
}

void PeerConnection::onGatheringStateChange(rtc::PeerConnection::GatheringState state)
{
	logger->debug("Gathering state changed: {}", state);
}

void PeerConnection::onSignalingConnected()
{
	logger->debug("Signaling connection established");
}

void PeerConnection::onSignalingDisconnected()
{
	logger->debug("Signaling connection closed");
}

void PeerConnection::onSignalingError(const std::string &err)
{
	logger->debug("Signaling connection error: {}", err);
}

void PeerConnection::onSignalingMessage(const SignalingMessage &msg)
{
	logger->debug("Signaling message received: {}", msg.toString());

	if (msg.type == SignalingMessage::TYPE_ANSWER || msg.type == SignalingMessage::TYPE_OFFER)
		conn->setRemoteDescription(*msg.description);

	if (msg.type == SignalingMessage::TYPE_CANDIDATE)
		conn->addRemoteCandidate(*msg.candidate);
}

void PeerConnection::onDataChannel(std::shared_ptr<rtc::DataChannel> dc)
{
	logger->debug("New data channel: id={}, stream={}, protocol={}, max_msg_size={}, label={}",
		dc->id(),
		dc->stream(),
		dc->protocol(),
		dc->maxMessageSize(),
		dc->label());

	dc->onMessage(std::bind(&PeerConnection::onDataChannelMessage, this, _1));
	dc->onOpen(std::bind(&PeerConnection::onDataChannelOpen, this));
	dc->onClosed(std::bind(&PeerConnection::onDataChannelClosed, this));
	dc->onError(std::bind(&PeerConnection::onDataChannelError, this, _1));
}

void PeerConnection::onDataChannelOpen()
{
	logger->debug("Datachannel opened");
}

void PeerConnection::onDataChannelClosed()
{
	logger->debug("Datachannel closed");
}

void PeerConnection::onDataChannelError(std::string err)
{
	logger->error("Datachannel error: {}", err);
}

void PeerConnection::onDataChannelMessage(rtc::message_variant msg)
{
	logger->debug("Datachannel message received");
}
