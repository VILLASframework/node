/** Node type: Universal Data-exchange API
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/node.hpp>
#include <pthread.h>

namespace villas {
namespace node {

/* Forward declarations */
struct Sample;

class APINode : public Node {

public:
	APINode(const std::string &name = "");

	struct Direction {
		Sample *sample;
		pthread_cond_t cv;
		pthread_mutex_t mutex;
	};

	// Accessed by api::universal::SignalRequest
	Direction read, write;

	virtual
	int prepare();

protected:

	virtual
	int _read(struct Sample *smps[], unsigned cnt);

	virtual
	int _write(struct Sample *smps[], unsigned cnt);
};

} /* namespace node */
} /* namespace villas */
