/** Node type: amqp
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <amqp.h>

#include <villas/list.hpp>
#include <villas/format.hpp>

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

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

	Format *formatter;
};

char * amqp_print(NodeCompat *n);

int amqp_parse(NodeCompat *n, json_t *json);

int amqp_start(NodeCompat *n);

int amqp_init(NodeCompat *n);

int amqp_destroy(NodeCompat *n);

int amqp_poll_fds(NodeCompat *n, int fds[]);

int amqp_stop(NodeCompat *n);

int amqp_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int amqp_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} /* namespace node */
} /* namespace villas */
