/** Node type: amqp
 *
 * This file implements the file type for nodes.
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
 * @addtogroup amqp amqp node type
 * @ingroup node
 * @{
 */

#pragma once

#include <amqp.h>

#include "node.h"
#include "list.h"

/* Forward declarations */
struct io_format;

struct amqp {
	struct amqp_connection_info connection_info;

	struct {
		amqp_socket_t *socket;
		amqp_connection_state_t connection;

		amqp_bytes_t routing_key;
		amqp_bytes_t exchange;
	} producer;

	struct {
		amqp_socket_t *socket;
		amqp_connection_state_t connection;

		amqp_bytes_t queue;
	} consumer;

	struct io_format *format;
};

/** @see node_type::print */
char * amqp_print(struct node *n);

/** @see node_type::parse */
int amqp_parse(struct node *n, json_t *cfg);

/** @see node_type::open */
int amqp_start(struct node *n);

/** @see node_type::close */
int amqp_stop(struct node *n);

/** @see node_type::read */
int amqp_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::write */
int amqp_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */
