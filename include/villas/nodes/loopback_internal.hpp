/** Node type: internal loopback
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/node.hpp>
#include <villas/queue_signalled.h>

namespace villas {
namespace node {

class InternalLoopbackNode : public Node {

protected:
	unsigned queuelen;
	struct CQueueSignalled queue;

	Node *source;

	virtual
	int _write(struct Sample * smps[], unsigned cnt);

	virtual
	int _read(struct Sample * smps[], unsigned cnt);

public:
	InternalLoopbackNode(Node *src, unsigned id = 0, unsigned ql = DEFAULT_QUEUE_LENGTH);

	virtual
	~InternalLoopbackNode();

	virtual
	std::vector<int> getPollFDs();

	virtual
	int stop();
};

class InternalLoopbackNodeFactory : public NodeFactory {

public:
	using NodeFactory::NodeFactory;

	virtual
	Node * make(const uuid_t &id = {}, const std::string &name = "")
	{
		return nullptr;
	}

	virtual
	int getFlags() const
	{
		return (int) NodeFactory::Flags::INTERNAL |
		       (int) NodeFactory::Flags::PROVIDES_SIGNALS |
		       (int) NodeFactory::Flags::SUPPORTS_READ |
		       (int) NodeFactory::Flags::SUPPORTS_WRITE |
		       (int) NodeFactory::Flags::SUPPORTS_POLL;
	}

	virtual
	std::string getName() const
	{
		return "loopback.internal";
	}

	virtual
	std::string getDescription() const
	{
		return "internal loopback node";
	}
};

} /* namespace node */
} /* namespace villas */
