/** Node type: nanomsg
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
 * @addtogroup nanomsg nanomsg node type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node.h>
#include <villas/list.h>
#include <villas/io.h>

/** The maximum length of a packet which contains stuct msg. */
#define NANOMSG_MAX_PACKET_LEN 1500

/* Forward declarations */
struct format_type;

struct nanomsg {
	struct {
		int socket;
		struct vlist endpoints;
	} in, out;

	struct format_type *format;
	struct io io;
};

/** @see node_type::print */
char * nanomsg_print(struct node *n);

/** @see node_type::parse */
int nanomsg_parse(struct node *n, json_t *cfg);

/** @see node_type::open */
int nanomsg_start(struct node *n);

/** @see node_type::close */
int nanomsg_stop(struct node *n);

/** @see node_type::read */
int nanomsg_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::write */
int nanomsg_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @} */
