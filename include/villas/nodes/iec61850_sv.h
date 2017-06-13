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

#include "node.h"
#include "list.h"

enum {
	IEC61850_TYPE_BOOLEAN,
	IEC61850_TYPE_INT8,
	IEC61850_TYPE_INT16,
	IEC61850_TYPE_INT32,
	IEC61850_TYPE_INT64,
	IEC61850_TYPE_INT8U,
	IEC61850_TYPE_INT16U,
	IEC61850_TYPE_INT24U,
	IEC61850_TYPE_INT32U,
	IEC61850_TYPE_FLOAT32,
	IEC61850_TYPE_FLOAT64,
	IEC61850_TYPE_ENUMERATED,
	IEC61850_TYPE_CODED_ENUM,
	IEC61850_TYPE_OCTET_STRING,
	IEC61850_TYPE_VISIBLE_STRING,
	IEC61850_TYPE_TIMESTAMP,
	IEC61850_TYPE_ENTRYTIME,
	IEC61850_TYPE_BITSTRING 
} type;

struct iec61850_sv_mapping {
	SV_ASDU *asdu;

	int offset;
	enum iec61850_type type;
};

struct iec61850_sv {
	char *interface;
	
	struct {
		SVReceiver receiver
		SVSubscriber subscriber;
	} in;

	struct {
		SampledValuesPublisher publisher;
	
		struct list mapping;
		struct list asdus;
	} out;
};

/** @} */