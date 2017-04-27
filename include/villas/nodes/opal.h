/** Node type: OPAL (libOpalAsync API)
 *
 * This file implements the opal subtype for nodes.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include "node.h"
#include "msg.h"

/* Define RTLAB before including OpalPrint.h for messages to be sent
 * to the OpalDisplay. Otherwise stdout will be used. */
#define RTLAB
#include "OpalPrint.h"
#include "AsyncApi.h"
#include "OpalGenAsyncParamCtrl.h"

struct opal {
	int reply;
	int mode;

	int send_id;
	int recv_id;
	
	Opal_SendAsyncParam send_params;
	Opal_RecvAsyncParam recv_params;
};

/** Initialize global OPAL settings and maps shared memory regions.
 *
 * @see node_type::init
 */
int opal_init(struct super_node *sn);

/** Free global OPAL settings and unmaps shared memory regions.
 *
 * @see node_type::deinit
 */
int opal_deinit();

/** @see node_type::parse */
int opal_parse(struct node *n, config_setting_t *cfg);

/** @see node_type::print */
char * opal_print(struct node *n);

/** Print global settings of the OPAL node type. */
int opal_print_global();

/** @see node_type::open */
int opal_start(struct node *n);

/** @see node_type::close */
int opal_stop(struct node *n);

/** @see node_type::read */
int opal_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int opal_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */