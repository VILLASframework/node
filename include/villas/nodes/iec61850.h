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

#ifdef __APPLE__
  #include <net/ethernet.h>
#else
  #include <netinet/ether.h>
#endif

#include <libiec61850/hal_ethernet.h>
#include <libiec61850/goose_receiver.h>
#include <libiec61850/sv_subscriber.h>

#include <villas/node.h>
#include <villas/list.h>

#ifdef __cplusplus
extern "C"{
#endif

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
	char format;
	enum iec61850_type type;
	unsigned size;
	bool publisher;
	bool subscriber;
};

struct iec61850_receiver {
	char *interface;

	EthernetSocket socket;

	enum iec61850_receiver_type {
		IEC61850_RECEIVER_GOOSE,
		IEC61850_RECEIVER_SV
	} type;

	union {
		SVReceiver sv;
		GooseReceiver goose;
	};
};

/** @see node_type::type_start */
int iec61850_type_start(struct super_node *sn);

/** @see node_type::type_stop */
int iec61850_type_stop();

const struct iec61850_type_descriptor * iec61850_lookup_type(const char *name, char fmt);

int iec61850_parse_mapping(json_t *json_mapping, struct list *mapping);

struct iec61850_receiver * iec61850_receiver_lookup(enum iec61850_receiver_type t, const char *intf);

struct iec61850_receiver * iec61850_receiver_create(enum iec61850_receiver_type t, const char *intf);

int iec61850_receiver_start(struct iec61850_receiver *r);

int iec61850_receiver_stop(struct iec61850_receiver *r);

int iec61850_receiver_destroy(struct iec61850_receiver *r);

#ifdef __cplusplus
}
#endif

/** @} */
