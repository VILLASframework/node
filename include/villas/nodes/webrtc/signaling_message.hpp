/** WebRTC signaling messages.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @author Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @copyright 2023, OPAL-RT Germany GmbH
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <string>
#include <chrono>
#include <vector>
#include <variant>

#include <rtc/rtc.hpp>
#include <jansson.h>

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
	json_t * toJSON() const;
};

struct RelayMessage {
	std::vector<rtc::IceServer> servers;

	RelayMessage(json_t *j);
};

struct ControlMessage {
	int peerID;
	std::vector<Peer> peers;

	ControlMessage(json_t *j);
	json_t * toJSON() const;
};

struct SignalingMessage {
	std::variant<std::monostate, RelayMessage, ControlMessage, rtc::Description, rtc::Candidate> message;

	static SignalingMessage fromJSON(json_t *j);
	json_t * toJSON() const;
	std::string toString() const;
};

} /* namespace webrtc */
} /* namespace node */
} /* namespace villas */
