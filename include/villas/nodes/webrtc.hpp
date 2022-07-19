/** Node-type webrtc.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <rtc/rtc.hpp>

#include <villas/nodes/webrtc/peer_connection.hpp>
#include <villas/node/config.hpp>
#include <villas/node.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

/* Forward declarations */
struct Sample;

class WebRTCNode : public Node {

protected:
	std::string server;
	std::string session;

	bool wait;
	bool ordered;
	int max_retransmits;

	std::shared_ptr<webrtc::PeerConnection> conn;
	rtc::Configuration config;

	virtual
	int _read(struct Sample *smps[], unsigned cnt);

	virtual
	int _write(struct Sample *smps[], unsigned cnt);

public:
	WebRTCNode(const std::string &name = "");

	/* All of the following virtual-declared functions are optional.
	 * Have a look at node.hpp/node.cpp for the default behaviour.
	 */

	virtual
	~WebRTCNode();

	virtual
	int prepare();

	virtual
	int parse(json_t *json, const uuid_t sn_uuid);

	/** Validate node configuration. */
	virtual
	int check();

	virtual
	int start();

	// virtual
	// int stop();

	// virtual
	// int pause();

	// virtual
	// int resume();

	// virtual
	// int restart();

	// virtual
	// int reverse();

	// virtual
	// std::vector<int> getPollFDs();

	// virtual
	// std::vector<int> getNetemFDs();

	// virtual
	// struct villas::node::memory::Type * getMemoryType();

	virtual
	const std::string & getDetails();
};


class WebRTCNodeFactory : public NodeFactory {

public:
	using NodeFactory::NodeFactory;

	virtual
	Node * make()
	{
		auto *n = new WebRTCNode();

		init(n);

		return n;
	}

	virtual
	int getFlags() const
	{
		return (int) NodeFactory::Flags::SUPPORTS_READ |
		       (int) NodeFactory::Flags::SUPPORTS_WRITE |
		       (int) NodeFactory::Flags::SUPPORTS_POLL |
		       (int) NodeFactory::Flags::REQUIRES_WEB;
	}

	virtual
	std::string getName() const
	{
		return "webrtc";
	}

	virtual
	std::string getDescription() const
	{
		return "Web Real-time Communication";
	}

	virtual
	int start(SuperNode *sn);
};


} /* namespace node */
} /* namespace villas */
