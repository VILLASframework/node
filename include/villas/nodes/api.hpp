/** Node type: Universal Data-exchange API
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
