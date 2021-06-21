/** Node type: amqp
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
 * @addtogroup amqp amqp node type
 * @ingroup node
 * @{
 */

#pragma once

#include <amqp.h>

#include <villas/list.h>
#include <villas/format.hpp>

/* Forward declarations */
struct vnode;

struct amqp_ssl_info {
	int verify_peer;
	int verify_hostname;
	char *ca_cert;
	char *client_cert;
	char *client_key;
};

struct amqp {
	char *uri;

	struct amqp_connection_info connection_info;
	struct amqp_ssl_info ssl_info;

	amqp_bytes_t routing_key;
	amqp_bytes_t exchange;

	/* We need to create two connection because rabbitmq-c is not thread-safe! */
	amqp_connection_state_t producer;
	amqp_connection_state_t consumer;

	villas::node::Format *formatter;
};

/** @see node_type::print */
char * amqp_print(struct vnode *n);

/** @see node_type::parse */
int amqp_parse(struct vnode *n, json_t *json);

/** @see node_type::start */
int amqp_start(struct vnode *n);

/** @see node_type::stop */
int amqp_stop(struct vnode *n);

/** @see node_type::read */
int amqp_read(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::write */
int amqp_write(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @} */
