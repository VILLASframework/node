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
#include <list>

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

struct ControlMessage {
	int connectionID;
	std::list<Connection> connections;

	ControlMessage(json_t *j);
	json_t * toJSON() const;
};

class SignalingMessage {
public:
	enum Type {
		TYPE_CONTROL,
		TYPE_OFFER,
		TYPE_ANSWER,
		TYPE_CANDIDATE
	};

	Type type;

	ControlMessage *control;
	rtc::Description *description;
	rtc::Candidate *candidate;
	std::string mid;

	SignalingMessage(rtc::Description desc, bool answer = false);
	SignalingMessage(rtc::Candidate cand);
	SignalingMessage(json_t *j);
	~SignalingMessage();

	json_t * toJSON() const;

	std::string toString() const;
};

} /* namespace webrtc */
} /* namespace node */
} /* namespace villas */
