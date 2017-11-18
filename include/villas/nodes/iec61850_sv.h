/** Node type: IEC 61850-9-2 (Sampled Values)
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
 * @addtogroup iec61850_sv IEC 61850-9-2 (Sampled Values) node type
 * @ingroup node
 * @{
 */

#pragma once

#include <stdint.h>

#include <libiec61850/sv_publisher.h>
#include <libiec61850/sv_subscriber.h>

#include "queue_signalled.h"
#include "pool.h"
#include "node.h"
#include "list.h"

#include "nodes/iec61850.h"

struct iec61850_sv_receiver {
	char *interface;
	SVReceiver receiver;
};

struct iec61850_sv {
	char *interface;
	int app_id;
	struct ether_addr dst_address;

	struct {
		SVSubscriber subscriber;
		SVReceiver receiver;

		struct queue_signalled queue;
		struct pool pool;

		struct list mapping;		/**< Mappings of type struct iec61850_type_descriptor */
		int total_size;
	} subscriber;

	struct {
		SVPublisher publisher;
		SVPublisher_ASDU asdu;

		char *datset;
		char *svid;

		int vlan_priority;
		int vlan_id;
		int smpmod;
		int smprate;
		int confrev;

		struct list mapping;		/**< Mappings of type struct iec61850_type_descriptor */
		int total_size;
	} publisher;
};

/** @} */
