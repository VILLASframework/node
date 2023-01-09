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

std::shared_ptr<rtc::PeerConnection> PeerConnection::createPeerConnection()
{
	logger->debug("Creating new peer connection");

	// Setup peer connection
	auto c = std::make_shared<rtc::PeerConnection>(config);

	c->onLocalDescription(std::bind(&PeerConnection::onLocalDescription, this, _1));
	c->onLocalCandidate(std::bind(&PeerConnection::onLocalCandidate, this, _1));

	c->onDataChannel(std::bind(&PeerConnection::onDataChannel, this, _1));
	c->onGatheringStateChange(std::bind(&PeerConnection::onGatheringStateChange, this, _1));
	c->onSignalingStateChange(std::bind(&PeerConnection::onSignalingStateChange, this, _1));
	c->onStateChange(std::bind(&PeerConnection::onConnectionStateChange, this, _1));

	return c;
}

std::shared_ptr<rtc::PeerConnection> PeerConnection::rollbackPeerConnection()
{
	logger->debug("Rolling-back peer connection");

	rollback = true;

	// Close previous peer connection in before creating a new one
	// We need to do this as libdatachannel currently does not support rollbacks.
	conn->close();

	auto c = createPeerConnection();

	rollback = false;

	return c;
}

std::shared_ptr<rtc::DataChannel> PeerConnection::createDatachannel()
{
	logger->debug("Creating new data channel");

	auto c = conn->createDataChannel("villas");

	c->onMessage(std::bind(&PeerConnection::onDataChannelMessage, this, _1));
	c->onOpen(std::bind(&PeerConnection::onDataChannelOpen, this));
	c->onClosed(std::bind(&PeerConnection::onDataChannelClosed, this));
	c->onError(std::bind(&PeerConnection::onDataChannelError, this, _1));

	return c;
}

void PeerConnection::onLocalDescription(rtc::Description sdp)
{
	logger->debug("New local description:\n{}", std::string(sdp));

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

	switch (state) {
	case rtc::PeerConnection::State::New:
		logger->info("New peer connection");
		break;

	case rtc::PeerConnection::State::Connecting:
		logger->info("Peer connection connecting.");
		break;

	case rtc::PeerConnection::State::Connected:
		logger->info("Peer connection connected.");
		break;

	case rtc::PeerConnection::State::Disconnected:
	case rtc::PeerConnection::State::Failed:
		logger->info("Closing peer connection");
		conn->close();

	case rtc::PeerConnection::State::Closed:
		if (rollback)
			return;

		logger->info("Closed peer connection");

		conn = createPeerConnection();

		onConnectionCreated();
	}
}

void PeerConnection::onConnectionCreated()
{
	if (!first)
		chan = createDatachannel();
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

	// Create initial peer connection
	if (!conn) {
		conn = createPeerConnection();

		onConnectionCreated();
	}
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

	switch (msg.type) {
	case SignalingMessage::TYPE_CONTROL:
		if (msg.control->connections.size() > 2) {
			logger->error("There are already two peers connected to this session.");
			return;
		}

		// The peer with the smallest connection ID connected first
		first = true;
		for (auto c : msg.control->connections) {
			if (c.id < msg.control->connectionID) {
				first = false;
			}
		}

		polite = first;

		logger->info("New role: polite={}, first={}", polite, first);
		break;

	case SignalingMessage::TYPE_ANSWER:
	case SignalingMessage::TYPE_OFFER: {
		auto readyForOffer = !makingOffer && conn->signalingState() == rtc::PeerConnection::SignalingState::Stable;
		auto offerCollision = msg.type == SignalingMessage::TYPE_OFFER && !readyForOffer;

		ignoreOffer = !polite && offerCollision;
		if (ignoreOffer)
			return;

		if (msg.type == SignalingMessage::TYPE_OFFER && conn->signalingState() != rtc::PeerConnection::SignalingState::Stable) {
			conn = rollbackPeerConnection();

			onConnectionCreated();
		}

		conn->setRemoteDescription(*(msg.description));

		break;
	}

	case SignalingMessage::TYPE_CANDIDATE:
		conn->addRemoteCandidate(*msg.candidate);
	}
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
