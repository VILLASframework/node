/** Node type: internal loopback
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
	Node * make()
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
