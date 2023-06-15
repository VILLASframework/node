/** WebRTC peer connection
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <chrono>
#include <thread>
#include <fmt/ostream.h>
#include <fmt/chrono.h>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/nodes/webrtc/peer_connection.hpp>

using namespace std::placeholders;

using namespace villas;
using namespace villas::node;
using namespace villas::node::webrtc;

PeerConnection::PeerConnection(const std::string &server, const std::string &session, rtc::Configuration cfg, Web *w, rtc::DataChannelInit d) :
	web(w),
	dataChannelInit(d),
	defaultConfig(cfg),
	logger(logging.get("webrtc:pc"))
{
	client = std::make_shared<SignalingClient>(server, session, web);
	client->onConnected([this](){ this->onSignalingConnected(); });
	client->onDisconnected([this](){ this->onSignalingDisconnected(); });
	client->onError([this](auto err){ this->onSignalingError(std::move(err)); });
	client->onMessage([this](auto msg){ this->onSignalingMessage(std::move(msg)); });

	auto lock = std::unique_lock { mutex };
	resetConnectionAndStandby(lock);
}

PeerConnection::~PeerConnection()
{
}

bool PeerConnection::waitForDataChannel(std::chrono::seconds timeout)
{
	auto lock = std::unique_lock { mutex };

	auto deadline = std::chrono::steady_clock::now() + timeout;

	return startupCondition.wait_until(lock, deadline, [this](){ return this->stopStartup; });
}

void PeerConnection::notifyStartup()
{
	stopStartup = true;
	startupCondition.notify_all();
}

void PeerConnection::onMessage(std::function<void(rtc::binary)> callback)
{
	auto lock = std::unique_lock { mutex };

	onMessageCallback = callback;
}

void PeerConnection::sendMessage(rtc::binary msg)
{
	auto lock = std::unique_lock { mutex };

	if (chan && chan->isOpen()) {
		chan->send(msg);
		warnNotConnected = true;
	} else if (warnNotConnected) {
		logger->warn("Dropping messages. No peer connected.");
		warnNotConnected = false;
	}
}

void PeerConnection::connect()
{
	client->connect();
}

void PeerConnection::disconnect()
{
	client->disconnect();
}

void PeerConnection::resetConnection(std::unique_lock<decltype(PeerConnection::mutex)> &lock)
{
	lock.unlock();
	chan.reset();
	conn.reset();
	lock.lock();
}

void PeerConnection::resetConnectionAndStandby(std::unique_lock<decltype(PeerConnection::mutex)> &lock)
{
	if (!standby)
		logger->info("Going to standby");

	standby = true;
	first = false;
	firstID = INT_MAX;
	secondID = INT_MAX;
	warnNotConnected = false;

	resetConnection(lock);
}

void PeerConnection::setupPeerConnection(std::shared_ptr<rtc::PeerConnection> pc)
{
	logger->debug("Setup {} peer connection", pc ? "existing" : "new");

	auto config = defaultConfig;
	config.iceServers.insert(std::end(config.iceServers), std::begin(extraServers), std::end(extraServers));

	conn = pc ? std::move(pc) : std::make_shared<rtc::PeerConnection>(config);
	conn->onLocalDescription([this](auto desc){ this->onLocalDescription(std::move(desc)); });
	conn->onLocalCandidate([this](auto cand){ this->onLocalCandidate(std::move(cand)); });
	conn->onDataChannel([this](auto channel){ this->onDataChannel(std::move(channel)); });
	conn->onGatheringStateChange([this](auto state){ this->onGatheringStateChange(std::move(state)); });
	conn->onSignalingStateChange([this](auto state){ this->onSignalingStateChange(std::move(state)); });
	conn->onStateChange([this](auto state){ this->onConnectionStateChange(std::move(state)); });
}

void PeerConnection::setupDataChannel(std::shared_ptr<rtc::DataChannel> dc)
{
	logger->debug("Setup {} data channel", dc ? "existing" : "new");

	assert(conn);
	chan = dc ? std::move(dc) : conn->createDataChannel("villas", dataChannelInit);
	chan->onMessage(
		[this](rtc::binary msg){ this->onDataChannelMessage(std::move(msg)); },
		[this](rtc::string msg){ this->onDataChannelMessage(std::move(msg)); }
	);
	chan->onOpen([this](){ this->onDataChannelOpen(); });
	chan->onClosed([this](){ this->onDataChannelClosed(); });
	chan->onError([this](auto err){ this->onDataChannelError(std::move(err)); });

	// if this node has it's data channel set up, don't accept any new ones
	conn->onDataChannel(nullptr);
}

void PeerConnection::onLocalDescription(rtc::Description desc)
{
	logger->debug("New local description: type={} sdp=\n{}", desc.typeString(), desc.generateSdp());

	auto lock = std::unique_lock { mutex };

	client->sendMessage({ desc });
}

void PeerConnection::onLocalCandidate(rtc::Candidate cand)
{
	logger->debug("New local candidate: {}", std::string { cand });

	auto lock = std::unique_lock { mutex };

	client->sendMessage({ cand });
}

void PeerConnection::onConnectionStateChange(rtc::PeerConnection::State state)
{
	//logger->debug("Connection State changed: {}", state);

	auto lock = std::unique_lock { mutex };

	switch (state) {
		case rtc::PeerConnection::State::New: {
			logger->debug("New peer connection");
			break;
		}

		case rtc::PeerConnection::State::Connecting: {
			logger->debug("Peer connection connecting.");
			break;
		}

		case rtc::PeerConnection::State::Connected: {
			rtc::Candidate local, remote;
			std::optional<std::chrono::milliseconds> rtt = conn->rtt();
			if (conn->getSelectedCandidatePair(&local, &remote)) {
				std::stringstream l, r;
				l << local, r << remote;
				logger->debug(
					"Peer connection connected:\n"
					"local: {}\n"
					"remote: {}\n"
					"bytes sent: {} / bytes received: {} / rtt: {}\n",
					l.str(),
					r.str(),
					conn->bytesSent(),
					conn->bytesReceived(),
					rtt.value_or(decltype(rtt)::value_type { 0 })
				);
			} else {
				logger->debug(
					"Peer connection connected.\n"
					"Could not get candidate pair info.\n"
				);
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

void PeerConnection::onSignalingStateChange(rtc::PeerConnection::SignalingState state)
{
	std::stringstream s;
	s << state;
	logger->debug("Signaling state changed: {}", s.str());
}

void PeerConnection::onGatheringStateChange(rtc::PeerConnection::GatheringState state)
{
	std::stringstream s;
	s << state;
	logger->debug("Gathering state changed: {}", s.str());
}

void PeerConnection::onSignalingConnected()
{
	logger->debug("Signaling connection established");
}

void PeerConnection::onSignalingDisconnected()
{
	logger->debug("Signaling connection closed");

	auto lock = std::unique_lock { mutex };

	resetConnectionAndStandby(lock);
}

void PeerConnection::onSignalingError(std::string err)
{
	logger->debug("Signaling connection error: {}", err);

	auto lock = std::unique_lock { mutex };

	resetConnectionAndStandby(lock);
}

void PeerConnection::onSignalingMessage(SignalingMessage msg)
{
	logger->debug("Signaling message received: {}", msg.toString());

	auto lock = std::unique_lock { mutex };

	std::visit(villas::utils::overloaded {
		[&](RelayMessage &c){
			extraServers = std::move(c.servers);
		},

		[&](ControlMessage &c){
			auto const &id = c.connectionID;

			if (c.connections.size() < 2) {
				resetConnectionAndStandby(lock);
				return;
			}

			auto fst = INT_MAX, snd = INT_MAX;
			for (auto &c : c.connections) {
				if (c.id < fst) {
					snd = fst;
					fst = c.id;
				} else if (c.id < snd) {
					snd = c.id;
				}
			}

			standby = (id != fst && id != snd);

			if (standby) {
				logger->error("There are already two peers connected to this session. Waiting in standby.");
				return;
			}

			if (fst == firstID && snd == secondID) {
				logger->debug("Ignoring control message. This connection is already being established.");
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

			logger->trace("New connection pair: first={}, second={}, I am {}", firstID, secondID, first ? "first" : "second");
		},

		[&](rtc::Description d){
			if (standby || !conn || (!first && d.type() == rtc::Description::Type::Offer))
				return;

			conn->setRemoteDescription(d);
		},

		[&](rtc::Candidate c){
			if (standby || !conn)
				return;

			conn->addRemoteCandidate(c);
		},

		[&](auto other){
			logger->warn("unknown signaling message");
		}
	}, msg.message);
}

void PeerConnection::onDataChannel(std::shared_ptr<rtc::DataChannel> dc)
{
	/*logger->debug("New data channel: id={}, stream={}, protocol={}, max_msg_size={}, label={}",
		dc->id(),
		dc->stream(),
		dc->protocol(),
		dc->maxMessageSize(),
		dc->label());
	*/

	auto lock = std::unique_lock { mutex };

	setupDataChannel(std::move(dc));
}

void PeerConnection::onDataChannelOpen()
{
	logger->debug("Datachannel opened");

	auto lock = std::unique_lock { mutex };

	chan->send("Hello from VILLASnode");

	notifyStartup();
}

void PeerConnection::onDataChannelClosed()
{
	logger->debug("Datachannel closed");

	auto lock = std::unique_lock { mutex };

	resetConnectionAndStandby(lock);
}

void PeerConnection::onDataChannelError(std::string err)
{
	logger->error("Datachannel error: {}", err);

	auto lock = std::unique_lock { mutex };

	resetConnectionAndStandby(lock);
}

void PeerConnection::onDataChannelMessage(rtc::string msg)
{
	logger->info("Received: {}", msg);
}

void PeerConnection::onDataChannelMessage(rtc::binary msg)
{
	logger->trace("Received binary data");

	auto lock = std::unique_lock { mutex };

	if (onMessageCallback)
		onMessageCallback(msg);
}
