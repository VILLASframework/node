/** Node-type: CAN bus
 *
 * @file
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <jansson.h>

#include <villas/timing.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;
union SignalData;

struct can_signal {
	uint32_t id;
	int offset;
	int size;
};

struct can {
	/* Settings */
	char *interface_name;
	double *sample_rate;
	struct can_signal *in;
	struct can_signal *out;

	/* States */
	int socket;
	union SignalData *sample_buf;
	size_t sample_buf_num;
	struct timespec start_time;
};

int can_init(NodeCompat *n);

int can_destroy(NodeCompat *n);

int can_parse(NodeCompat *n, json_t *json);

char * can_print(NodeCompat *n);

int can_check(NodeCompat *n);

int can_prepare(NodeCompat *n);

int can_start(NodeCompat *n);

int can_stop(NodeCompat *n);

int can_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int can_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int can_poll_fds(NodeCompat *n, int fds[]);

} /* namespace node */
} /* namespace villas */
