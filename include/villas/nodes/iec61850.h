/** Some helpers to libiec61850
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

#include "node.h"
#include "list.h"

/* libiec61850's API is ugly... */
#define SVPublisher			SampledValuesPublisher
#define SVPublisher_create		SampledValuesPublisher_create
#define SVPublisher_destroy		SampledValuesPublisher_destroy
#define SVPublisher_addASDU		SampledValuesPublisher_addASDU
#define SVPublisher_setupComplete	SampledValuesPublisher_setupComplete
#define SVPublisher_publish		SampledValuesPublisher_publish

#define SVPublisher_ASDU		SV_ASDU
#define SVSubscriber_ASDU		SVClientASDU

#define SVPublisher_ASDU_setINT8	SV_ASDU_setINT8
#define SVPublisher_ASDU_setINT32	SV_ASDU_setINT32
#define SVPublisher_ASDU_setFLOAT32	SV_ASDU_setFLOAT
#define SVPublisher_ASDU_setFLOAT64	SV_ASDU_setFLOAT64

#define SVPublisher_ASDU_increaseSmpCnt	SV_ASDU_increaseSmpCnt
#define SVPublisher_ASDU_setSmpCnt	SV_ASDU_setSmpCnt
#define SVPublisher_ASDU_setRefrTm	SV_ASDU_setRefrTm

#define SVSubscriber_ASDU_getDataSize	SVClientASDU_getDataSize
#define SVSubscriber_ASDU_getConfRev	SVClientASDU_getConfRev
#define SVSubscriber_ASDU_getSvId	SVClientASDU_getSvId
#define SVSubscriber_ASDU_getSmpCnt	SVClientASDU_getSmpCnt
#define SVSubscriber_ASDU_getRefrTmAsMs	SVClientASDU_getRefrTmAsMs
#define SVSubscriber_ASDU_hasRefrTm	SVClientASDU_hasRefrTm

#define SVSubscriber_ASDU_getINT8	SVClientASDU_getINT8
#define SVSubscriber_ASDU_getINT16	SVClientASDU_getINT16
#define SVSubscriber_ASDU_getINT32	SVClientASDU_getINT32
#define SVSubscriber_ASDU_getINT8U	SVClientASDU_getINT8U
#define SVSubscriber_ASDU_getINT16U	SVClientASDU_getINT16U
#define SVSubscriber_ASDU_getINT32U	SVClientASDU_getINT32U
#define SVSubscriber_ASDU_getFLOAT32	SVClientASDU_getFLOAT32
#define SVSubscriber_ASDU_getFLOAT64	SVClientASDU_getFLOAT64


enum iec61850_type {
	/* According to IEC 61850-7-2 */
	IEC61850_TYPE_BOOLEAN,
	IEC61850_TYPE_INT8,
	IEC61850_TYPE_INT16,
	IEC61850_TYPE_INT32,
	IEC61850_TYPE_INT64,
	IEC61850_TYPE_INT8U,
	IEC61850_TYPE_INT16U,
	IEC61850_TYPE_INT32U,
	IEC61850_TYPE_INT64U,
	IEC61850_TYPE_FLOAT32,
	IEC61850_TYPE_FLOAT64,
	IEC61850_TYPE_ENUMERATED,
	IEC61850_TYPE_CODED_ENUM,
	IEC61850_TYPE_OCTET_STRING,
	IEC61850_TYPE_VISIBLE_STRING,
	IEC61850_TYPE_OBJECTNAME,
	IEC61850_TYPE_OBJECTREFERENCE,
	IEC61850_TYPE_TIMESTAMP,
	IEC61850_TYPE_ENTRYTIME,

	/* According to IEC 61850-8-1 */
	IEC61850_TYPE_BITSTRING
};

struct iec61850_type_descriptor {
	const char *name;
	enum iec61850_type type;
	unsigned size;
	bool publisher;
	bool subscriber;
};

/** @} */
