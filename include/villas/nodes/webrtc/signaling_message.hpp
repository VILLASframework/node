/** WebRTC signaling messages.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

struct Connection {
	int id;
	std::string remote;
	std::string userAgent;
	std::chrono::time_point<std::chrono::system_clock> created;

	Connection(json_t *j);
	json_t * toJSON() const;
};

struct RelayMessage {
	std::vector<rtc::IceServer> servers;

	RelayMessage(json_t *j);
};

struct ControlMessage {
	int connectionID;
	std::vector<Connection> connections;

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
