/** Node-type for loopback connections.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <jansson.h>

#include <villas/queue_signalled.h>

namespace villas {
namespace node {

class LoopbackNode : public Node {

protected:
	int queuelen;
	struct CQueueSignalled queue;
	enum QueueSignalledMode mode;

	virtual
	int _write(struct Sample * smps[], unsigned cnt);

	virtual
	int _read(struct Sample * smps[], unsigned cnt);

public:
	LoopbackNode(const std::string &name = "");

	virtual
	~LoopbackNode();

	virtual
	int prepare();

	virtual
	int stop();

	virtual
	std::vector<int> getPollFDs();

	virtual
	const std::string & getDetails();

	virtual
	int parse(json_t *json);
};

} /* namespace node */
} /* namespace villas */
