/** An example get started with new implementations of new node-types
 *
 * This example does not do any particulary useful.
 * It is just a skeleton to get you started with new node-types.
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

#include <villas/node/config.hpp>
#include <villas/node.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

/* Forward declarations */
struct Sample;

class ExampleNode : public Node {

protected:
	/* Place any configuration and per-node state here */

	/* Settings */
	int setting1;

	std::string setting2;

	/* States */
	int state1;
	struct timespec start_time;

	virtual
	int _read(struct Sample *smps[], unsigned cnt);

	virtual
	int _write(struct Sample *smps[], unsigned cnt);

public:
	ExampleNode(const std::string &name = "");

	/* All of the following virtual-declared functions are optional.
	 * Have a look at node.hpp/node.cpp for the default behaviour.
	 */

	virtual
	~ExampleNode();

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
	std::string getDetails();
};


} /* namespace node */
} /* namespace villas */
