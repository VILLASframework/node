/** Node-type webrtc.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @author Philipp Jungkamp <Philipp.Jungkamp@opal-rt.com>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @copyright 2023, OPAL-RT Germany GmbH
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <rtc/rtc.hpp>

#include <villas/nodes/webrtc/peer_connection.hpp>
#include <villas/node/config.hpp>
#include <villas/node.hpp>
#include <villas/timing.hpp>
#include <villas/format.hpp>
#include <villas/queue_signalled.h>

namespace villas {
namespace node {

/* Forward declarations */
struct Sample;

class WebRTCNode : public Node {

protected:
	std::string server;
	std::string session;
	std::string peer;

	int wait_seconds;
	Format *format;
	struct CQueueSignalled queue;
	struct Pool pool;

	std::shared_ptr<webrtc::PeerConnection> conn;
	rtc::Configuration rtcConf;
	rtc::DataChannelInit dci;

	virtual
	int _read(struct Sample *smps[], unsigned cnt);

	virtual
	int _write(struct Sample *smps[], unsigned cnt);

	virtual
	json_t * _readStatus() const;

public:
	WebRTCNode(const uuid_t &id = {}, const std::string &name = "");

	virtual
	~WebRTCNode();

	virtual
	int prepare();

	virtual
	int parse(json_t *json);

	virtual
	int start();

	virtual
	int stop();

	virtual
	std::vector<int> getPollFDs();

	virtual
	const std::string & getDetails();
};


class WebRTCNodeFactory : public NodeFactory {

public:
	using NodeFactory::NodeFactory;

	virtual
	Node * make(const uuid_t &id = {}, const std::string &nme = "")
	{
		auto *n = new WebRTCNode(id, nme);

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


} // namespace node
} // namespace villas
