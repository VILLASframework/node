/** WebRTC signaling messages.
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <fmt/format.h>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/nodes/webrtc/signaling_message.hpp>
#include <algorithm>

using namespace villas;
using namespace villas::node;
using namespace villas::node::webrtc;


json_t * Connection::toJSON() const
{
	return json_pack("{ s:i, s:s, s:s, s:s }",
		"id", id,
		"remote", remote.c_str(),
		"user_agent", userAgent.c_str(),
		"created", "" // TODO
	);
}

Connection::Connection(json_t *json)
{
	const char *rem, *ua, *ts;

	int ret = json_unpack(json, "{ s:i, s:s, s:s, s:s }",
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

RelayMessage::RelayMessage(json_t *json)
{

	if (!json_is_array(json))
		throw RuntimeError("Failed to decode signaling message");

	int ret;
	char *url;
	char *user;
	char *pass;
	char *realm;
	char *expires;
	json_t *server_json;
	size_t i;
	json_array_foreach(json, i, server_json) {
		ret = json_unpack(server_json, "{ s:s, s:s, s:s, s:s, s:s }",
			"url", &url,
			"user", &user,
			"pass", &pass,
			"realm", &realm,
			"expires", &expires
		);
		if (ret)
			throw RuntimeError("Failed to decode signaling message");

		auto &server = servers.emplace_back(url);
		server.username = user;
		server.password = pass;

		// TODO warn about unsupported realm
		// TODO log info expires time
	}
}

json_t * ControlMessage::toJSON() const
{
	json_t *json_connections = json_array();

	for (auto &c : connections) {
		json_t *json_connection = c.toJSON();

		json_array_append_new(json_connections, json_connection);
	}

	return json_pack("{ s:i, s:o }",
		"connection_id", connectionID,
		"connections", json_connections
	);
}

ControlMessage::ControlMessage(json_t *j)
{
	int ret;

	json_t *json_connections;

	ret = json_unpack(j, "{ s:i, s:o }",
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
	return std::visit(villas::utils::overloaded {
		[](ControlMessage const &c){
			return json_pack("{ s:o }", "control", c.toJSON());
		},
		[](rtc::Description const &d){
			return json_pack("{ s:{ s:s, s:s } }", "description",
				"spd", d.generateSdp().c_str(),
				"type", d.typeString().c_str()
			);
		},
		[](rtc::Candidate const &c){
			return json_pack("{ s:{ s:s, s:s } }", "candidate",
				"spd", c.candidate().c_str(),
				"mid", c.mid().c_str()
			);
		},
		[](auto &other){
			return (json_t *) { nullptr };
		}
	}, *this);
}

std::string SignalingMessage::toString() const
{
	return std::visit(villas::utils::overloaded {
		[](RelayMessage const &r){
			return fmt::format("type=relay");
		},
		[](ControlMessage const &c){
			return fmt::format("type=control, control={}", json_dumps(c.toJSON(), 0));
		},
		[](rtc::Description const &d){
			return fmt::format("type=description, type={}, spd=\n{}", d.typeString(), d.generateSdp());
		},
		[](rtc::Candidate const &c){
			return fmt::format("type=candidate, mid={}, spd=\n{}", c.candidate(), c.mid());
		},
		[](auto other){
			return fmt::format("invalid signaling message");
		}
	}, *this);
}

SignalingMessage::SignalingMessage(json_t *json)
{
	// relay message
	json_t *rlys = nullptr;
	// control message
	json_t *ctrl = nullptr;
	// candidate message
	const char *cand = nullptr;
	const char *mid = nullptr;
	// description message
	const char *desc = nullptr;
	const char *typ = nullptr;

	int ret = json_unpack(json, "{ s?o, s?o, s?{ s:s, s:s }, s?{ s:s, s:s } }",
		"servers", &rlys,
		"control", &ctrl,
		"candidate",
			"spd", &cand,
			"mid", &mid,
		"description",
			"spd", &desc,
			"type", &typ
	);

	// exactly 1 field may be specified
	const void *fields[] = { ctrl, cand, desc };
	if (ret || std::count(std::begin(fields), std::end(fields), nullptr) < std::make_signed_t<size_t>(std::size(fields)) - 1)
		throw RuntimeError("Failed to decode signaling message");

	if (rlys) {
		emplace<RelayMessage>(rlys);
	}
	else if (ctrl) {
		emplace<ControlMessage>(ctrl);
	}
	else if (cand) {
		emplace<rtc::Candidate>(cand, mid);
	}
	else if (desc) {
		emplace<rtc::Description>(desc, typ);
	}
}
