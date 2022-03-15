/** Node-type for InfluxDB.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <villas/list.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;
struct Sample;

struct influxdb {
	char *host;
	char *port;
	char *key;

	struct List fields;

	int sd;
};

char * influxdb_print(NodeCompat *n);

int influxdb_parse(NodeCompat *n, json_t *json);

int influxdb_open(NodeCompat *n);

int influxdb_close(NodeCompat *n);

int influxdb_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} /* namespace node */
} /* namespace villas */
