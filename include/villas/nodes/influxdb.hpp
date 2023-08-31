/* Node-type for InfluxDB.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/list.hpp>

namespace villas {
namespace node {

// Forward declarations
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

} // namespace node
} // namespace villas
