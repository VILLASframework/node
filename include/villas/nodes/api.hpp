/** Node type: Universal Data-exchange API (v2)
 *
 * @see https://github.com/ERIGrid2/JRA-3.1-api
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/node.hpp>
#include <villas/api/universal.hpp>
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
		api::universal::ChannelList channels;
		pthread_cond_t cv;
		pthread_mutex_t mutex;
	};

	// Accessed by api::universal::SignalRequest
	Direction read, write;

	virtual
	int prepare();

	virtual
	int check();

protected:
	virtual
	int parse(json_t *json, const uuid_t sn_uuid);

	virtual
	int _read(struct Sample *smps[], unsigned cnt);

	virtual
	int _write(struct Sample *smps[], unsigned cnt);
};

} /* namespace node */
} /* namespace villas */
