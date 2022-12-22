/** Node-type for loopback connections.
 *
 * @file
 * @author Manuel Pitz <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <jansson.h>
#include <villas/node.hpp>
#include <villas/queue_signalled.h>

namespace villas {
namespace node {

class SMUNode : public Node {

private:
	float counter;

protected:
	virtual
	int _read(struct Sample * smps[], unsigned cnt);

public:
	SMUNode(const std::string &name = "");

	virtual
	int prepare();

	virtual
	int start();
};

} /* namespace node */
} /* namespace villas */
