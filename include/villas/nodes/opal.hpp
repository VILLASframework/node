/** Node type: OPAL (libOpalAsync API)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

/**
 * @ingroup node
 * @addtogroup opal OPAL-RT Async Process node type
 * @{
 */

#pragma once

#include <pthread.h>

#include <villas/node.h>
#include <villas/sample.h>

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

/** Initialize global OPAL settings and maps shared memory regions.
 *
 * @see node_type::type_start
 */
int opal_type_start(villas::node::SuperNode *sn);

int opal_register_region(int argc, char *argv[]);

/** Free global OPAL settings and unmaps shared memory regions.
 *
 * @see node_type::type_stop
 */
int opal_type_stop();

/** @see node_type::parse */
int opal_parse(struct vnode *n, json_t *json);

/** @see node_type::print */
char * opal_print(struct vnode *n);

/** Print global settings of the OPAL node type. */
int opal_print_global();

/** @see node_type::start */
int opal_start(struct vnode *n);

/** @see node_type::stop */
int opal_stop(struct vnode *n);

/** @see node_type::read */
int opal_read(struct vnode *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int opal_write(struct vnode *n, struct sample *smps[], unsigned cnt);

/** @} */
