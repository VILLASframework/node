/** Node type: OPAL (libOpalAsync API)
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
