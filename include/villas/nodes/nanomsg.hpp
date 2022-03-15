/** Node type: nanomsg
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
#include <villas/format.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

/** The maximum length of a packet which contains stuct msg. */
#define NANOMSG_MAX_PACKET_LEN 1500

struct nanomsg {
	struct {
		int socket;
		struct List endpoints;
	} in, out;

	Format *formatter;
};

char * nanomsg_print(NodeCompat *n);

int nanomsg_init(NodeCompat *n);

int nanomsg_destroy(NodeCompat *n);

int nanomsg_parse(NodeCompat *n, json_t *json);

int nanomsg_start(NodeCompat *n);

int nanomsg_stop(NodeCompat *n);

int nanomsg_type_stop();

int nanomsg_reverse(NodeCompat *n);

int nanomsg_poll_fds(NodeCompat *n, int fds[]);

int nanomsg_netem_fds(NodeCompat *n, int fds[]);

int nanomsg_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int nanomsg_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} /* namespace node */
} /* namespace villas */
