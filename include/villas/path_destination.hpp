/** Path destination
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

#include <memory>

#include <villas/queue.h>

namespace villas {
namespace node {

/* Forward declarations */
class Node;
class Path;
struct Sample;
class PathDestination;

class PathDestination {
	friend Path;

public:
	using Ptr = std::shared_ptr<PathDestination>;

protected:
	Node *node;
	Path *path;

	struct CQueue queue;

public:
	PathDestination(Path *p, Node *n);

	~PathDestination();

	int prepare(int queuelen);

	void check();

	static
	void enqueueAll(class Path *p, const struct Sample * const smps[], unsigned cnt);

	void write();

	Node * getNode() const
	{
		return node;
	}
};

using PathDestinationList = std::vector<PathDestination::Ptr>;

} /* namespace node */
} /* namespace villas */
