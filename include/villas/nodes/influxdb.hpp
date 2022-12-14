/** Node-type for InfluxDB.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
