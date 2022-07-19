/** WebRTC signaling messages.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/exceptions.hpp>
#include <villas/nodes/webrtc/signaling_message.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::node::webrtc;


json_t * Connection::toJSON() const
{
	return json_pack("{ s: i, s: s, s: s, s: s }",
		"id", id,
		"remote", remote.c_str(),
		"user_agent", userAgent.c_str(),
		"created", "" // TODO
	);
}

Connection::Connection(json_t *j)
{
	const char *rem, *ua, *ts;

	int ret = json_unpack(j, "{ s: i, s: s, s: s, s: s }",
		"id", &id,
		"remote", &rem,
		"user_agent", &ua,
		"created", &ts
	);
	if (ret)
		throw RuntimeError("Failed to decode signaling message");

	remote = rem;
	userAgent = ua;

	// TODO: created
}

json_t * ControlMessage::toJSON() const
{
	json_t *json_connections = json_array();

	for (auto &c : connections) {
		json_t *json_connection = c.toJSON();

		json_array_append(json_connections, json_connection);
	}

	return json_pack("{ s: i, s: o }",
		"connection_id", connectionID,
		"connections", json_connections
	);
}

ControlMessage::ControlMessage(json_t *j)
{
	int ret;

	json_t *json_connections;

	ret = json_unpack(j, "{ s: i, s: o }",
		"connection_id", &connectionID,
		"connections", &json_connections
	);
	if (ret)
		throw RuntimeError("Failed to decode signaling message");

	if (!json_is_array(json_connections))
		throw RuntimeError("Failed to decode signaling message");

	json_t *json_connection;
	size_t i;
	json_array_foreach(json_connections, i, json_connection)
		connections.emplace_back(json_connection);
}

json_t * SignalingMessage::toJSON() const
{
	switch (type) {
	case TYPE_CONTROL:
		return json_pack("{ s: s, s: o }",
			"type", "control",
			"control", control->toJSON()
		);

	case TYPE_OFFER:
	case TYPE_ANSWER:
		return json_pack("{ s: s, s: o }",
			"type", type == TYPE_OFFER ? "offer" : "answer",
			"description", std::string(*description).c_str()
		);

	case TYPE_CANDIDATE:
		return json_pack("{ s: s, s: o }",
			"type", "candidate",
			"candidate", std::string(*candidate).c_str()
		);
	}

	return nullptr;
}

std::string SignalingMessage::toString() const
{
	switch (type) {
		case TYPE_ANSWER:
			return fmt::format("type=answer, description={}", std::string(*description));

		case TYPE_OFFER:
			return fmt::format("type=offer, description={}", std::string(*description));

		case TYPE_CANDIDATE:
			return fmt::format("type=candidate, candidate={}", std::string(*candidate));

		case TYPE_CONTROL:
			return fmt::format("type=control, control={}", json_dumps(control->toJSON(), 0));
	}

	return "";
}

SignalingMessage::SignalingMessage(rtc::Description desc, bool answer) :
	type(answer ? TYPE_ANSWER : TYPE_OFFER),
	description(new rtc::Description(desc))
{ }

SignalingMessage::SignalingMessage(rtc::Candidate cand) :
	type(TYPE_CANDIDATE),
	candidate(new rtc::Candidate(cand))
{ }

SignalingMessage::~SignalingMessage()
{
	if (description)
		delete description;

	if (candidate)
		delete candidate;

	if (control)
		delete control;
}

SignalingMessage::SignalingMessage(json_t *j) :
	control(nullptr),
	description(nullptr),
	candidate(nullptr)
{
	json_t *json_control = nullptr;
	const char *desc = nullptr;
	const char *cand = nullptr;
	const char *typ = nullptr;

	int ret = json_unpack(j, "{ s: s, s?: s, s?: s, s?: o }",
		"type", &typ,
		"description", &desc,
		"candidate", &cand,
		"control", &json_control
	);
	if (ret)
		throw RuntimeError("Failed to decode signaling message");

	if      (!strcmp(typ, "offer")) {
		type = TYPE_OFFER;
		description = new rtc::Description(desc);
	}
	else if (!strcmp(typ, "answer")) {
		type = TYPE_ANSWER;
		description = new rtc::Description(desc);
	}
	else if (!strcmp(typ, "candidate")) {
		type = TYPE_CANDIDATE;
		candidate = new rtc::Candidate(cand);
	}
	else if (!strcmp(typ, "control")) {
		type = TYPE_CONTROL;
		control = new ControlMessage(json_control);
	}
}
