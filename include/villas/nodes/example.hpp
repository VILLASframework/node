/** An example get started with new implementations of new node-types
 *
 * This example does not do any particulary useful.
 * It is just a skeleton to get you started with new node-types.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
	ExampleNode(const uuid_t &id = {}, const std::string &name = "");

	/* All of the following virtual-declared functions are optional.
	 * Have a look at node.hpp/node.cpp for the default behaviour.
	 */

	virtual
	~ExampleNode();

	virtual
	int prepare();

	virtual
	int parse(json_t *json);

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


} // namespace node
} // namespace villas
