/** Node-type for InfluxDB.
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
 * @addtogroup influxdb InfluxDB node-type
 * @{
 */

#pragma once

#include <villas/list.h>

/* Forward declarations */
struct vnode;
struct sample;

/** Node-type for signal generation.
 * @see node_type
 */
struct influxdb {
	char *host;
	char *port;
	char *key;

	struct vlist fields;

	int sd;
};

/** @see node_type::print */
char * influxdb_print(struct vnode *n);

/** @see node_type::parse */
int influxdb_parse(struct vnode *n, json_t *json);

/** @see node_type::start */
int influxdb_open(struct vnode *n);

/** @see node_type::stop */
int influxdb_close(struct vnode *n);

/** @see node_type::write */
int influxdb_write(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @} */
