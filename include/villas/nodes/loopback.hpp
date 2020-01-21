/** Node-type for loopback connections.
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
 * @addtogroup loopback Loopback connections
 * @{
 */

#pragma once

#include <villas/queue_signalled.h>
#include <villas/pool.h>

/* Forward declarations */
struct node;
struct sample;

/** Node-type for signal generation.
 * @see node_type
 */
struct loopback {
	int queuelen;
	enum QueueSignalledMode mode;
	struct queue_signalled queue;
	struct pool pool;
};

/** @see node_type::print */
char * loopback_print(struct node *n);

/** @see node_type::parse */
int loopback_parse(struct node *n, json_t *cfg);

/** @see node_type::start */
int loopback_open(struct node *n);

/** @see node_type::stop */
int loopback_close(struct node *n);

/** @see node_type::read */
int loopback_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::write */
int loopback_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @} */
