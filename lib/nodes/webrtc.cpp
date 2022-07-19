/** Node-type: webrtc
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <vector>

#include <villas/node_compat.hpp>
#include <villas/nodes/webrtc.hpp>
#include <villas/utils.hpp>
#include <villas/sample.hpp>
#include <villas/exceptions.hpp>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

static villas::node::Web *web;

WebRTCNode::WebRTCNode(const std::string &name) :
	Node(name),
	server("wss://villas.k8s.eonerc.rwth-aachen.de/ws/signaling"),
	wait(true),
	ordered(false),
	max_retransmits(0)
{
	config.iceServers = {
		rtc::IceServer("stun://stun.l.google.com:19302"),
		rtc::IceServer("stun://villas:villas@stun.0l.de"),
		rtc::IceServer("turn://villas:villas@turn.0l.de?transport=udp"),
		rtc::IceServer("turn://villas:villas@turn.0l.de?transport=tcp"),
	};
}

WebRTCNode::~WebRTCNode()
{ }

int WebRTCNode::parse(json_t *json, const uuid_t sn_uuid)
{
	int ret = Node::parse(json, sn_uuid);
	if (ret)
		return ret;

	const char *sess;
	const char *svr = nullptr;
	int w = - 1, ord = -1;
	json_t *json_ice = nullptr;

	json_error_t err;
	ret = json_unpack_ex(json, &err, 0, "{ s: s, s?: s, s?: b, s?: i, s?: b, s?: o }",
		"session", &sess,
		"server", &svr,
		"wait", &w,
		"max_retransmits", &max_retransmits,
		"ordered", &ord,
		"ice", &json_ice
	);
	if (ret)
		throw ConfigError(json, err, "node-config-node-webrtc");

	session = sess;

	if (svr)
		server = svr;

	if (w >= 0)
		wait = w != 0;

	if (ord >= 0)
		ordered = ordered != 0;

	if (json_ice) {
		json_t *json_servers = nullptr;

		ret = json_unpack_ex(json_ice, &err, 0, "{ s?: o }",
			"servers", &json_servers
		);
		if (ret)
			throw ConfigError(json, err, "node-config-node-webrtc-ice");

		if (json_servers) {
			config.iceServers.clear();

			if (!json_is_array(json_servers))
				throw ConfigError(json_servers, "node-config-node-webrtc-ice-servers", "ICE Servers must be a an array of server configurations.");

			size_t i;
			json_t *json_server;
			json_array_foreach(json_servers, i, json_server) {
				if (!json_is_string(json_server))
					throw ConfigError(json_server, "node-config-node-webrtc-ice-server", "ICE servers must be provided as STUN/TURN url.");

				std::string uri = json_string_value(json_server);

				config.iceServers.emplace_back(uri);
			}
		}
	}

	return 0;
}

int WebRTCNode::check()
{
	return 0;
}

int WebRTCNode::prepare()
{
	conn = std::make_shared<webrtc::PeerConnection>(server, session, config, web);

	return Node::prepare();
}

int WebRTCNode::start()
{
	conn->connect();

	if (wait) {
		logger->info("Wait until datachannel is connected...");

		conn->waitForDataChannel();
	}

	int ret = Node::start();
	if (!ret)
		state = State::STARTED;

	return 0;
}

// int WebRTCNode::stop()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int WebRTCNode::pause()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int WebRTCNode::resume()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int WebRTCNode::restart()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// int WebRTCNode::reverse()
// {
// 	// TODO add implementation here
// 	return 0;
// }

// std::vector<int> WebRTCNode::getPollFDs()
// {
// 	// TODO add implementation here
// 	return {};
// }

// std::vector<int> WebRTCNode::getNetemFDs()
// {
// 	// TODO add implementation here
// 	return {};
// }

// struct villas::node::memory::Type * WebRTCNode::getMemoryType()
// {
//	// TODO add implementation here
// }

const std::string & WebRTCNode::getDetails()
{
	details = fmt::format("");
	return details;
}

int WebRTCNode::_read(struct Sample *smps[], unsigned cnt)
{
	int read;
	struct timespec now;

	/* TODO: Add implementation here. The following is just an example */

	assert(cnt >= 1 && smps[0]->capacity >= 1);

	now = time_now();

	// smps[0]->data[0].f = time_delta(&now, &start_time);

	/* Dont forget to set other flags in struct Sample::flags
	 * E.g. for sequence no, timestamps... */
	smps[0]->flags = (int) SampleFlags::HAS_DATA;
	smps[0]->signals = getInputSignals(false);

	read = 1; /* The number of samples read */

	return read;
}

int WebRTCNode::_write(struct Sample *smps[], unsigned cnt)
{
	int written;

	/* TODO: Add implementation here. */

	written = 0; /* The number of samples written */

	return written;
}

int WebRTCNodeFactory::start(SuperNode *sn)
{
	web = sn->getWeb();
	if (!web->isEnabled())
		return -1;

	return 0;
}


static WebRTCNodeFactory p;
