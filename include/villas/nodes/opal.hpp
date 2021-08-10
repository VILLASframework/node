/** Node type: OPAL (libOpalAsync API)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <pthread.h>

#include <villas/sample.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

extern "C" {
	#include <OpalGenAsyncParamCtrl.h>
}

struct opal {
	int reply;
	int mode;
	int sequenceNo;

	unsigned sendID;
	unsigned recvID;

	Opal_SendAsyncParam sendParams;
	Opal_RecvAsyncParam recvParams;
};

int opal_type_start(SuperNode *sn);

int opal_register_region(int argc, char *argv[]);

int opal_type_stop();

int opal_parse(NodeCompat *n, json_t *json);

char * opal_print(NodeCompat *n);

int opal_start(NodeCompat *n);

int opal_stop(NodeCompat *n);

int opal_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int opal_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} /* namespace node */
} /* namespace villas */
