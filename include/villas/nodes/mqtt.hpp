/** Node type: mqtt
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
 * @addtogroup mqtt mqtt node type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/pool.h>
#include <villas/format.hpp>
#include <villas/queue_signalled.h>

/* Forward declarations */
struct vnode;
struct mosquitto;

struct mqtt {
	struct mosquitto *client;
	struct queue_signalled queue;
	struct pool pool;

	int keepalive;		/**< Keep-alive interval in seconds. Zero for no keepalive. */
	int port;		/**< Hostname / IP address of the broker. */
	int qos;		/**< Integer value 0, 1 or 2 indicating the Quality of Service to be used for publishing messages. */
	int retain;		/**< Mark published messages as retained. */
	char *host;		/**< Hostname / IP address of the broker. */
	char *username;		/**< Username for authentication to the broker. */
	char *password;		/**< Password for authentication to the broker. */
	char *publish;		/**< Publish topic. */
	char *subscribe;	/**< Subscribe topic. */

	struct {
		int enabled;	/**< Enable SSL encrypted connection to broker. */
		int insecure;	/**< Allow insecure SSL connections. */
		char *cafile;	/**< SSL CA file. */
		char *capath;	/**< SSL CA path. */
		char *certfile;	/**< SSL certificate. */
		char *keyfile;	/**< SSL private key. */
	} ssl;

	villas::node::Format *formatter;
};

/** @see node_type::reverse */
int mqtt_reverse(struct vnode *n);

/** @see node_type::print */
char * mqtt_print(struct vnode *n);

/** @see node_type::prepare */
int mqtt_prepare(struct vnode *n);

/** @see node_type::parse */
int mqtt_parse(struct vnode *n, json_t *json);

/** @see node_type::start */
int mqtt_start(struct vnode *n);

/** @see node_type::destroy */
int mqtt_destroy(struct vnode *n);

/** @see node_type::stop */
int mqtt_stop(struct vnode *n);

/** @see node_type::type_start */
int mqtt_type_start(villas::node::SuperNode *sn);

/** @see node_type::type_stop */
int mqtt_type_stop();

/** @see node_type::read */
int mqtt_read(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::write */
int mqtt_write(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @} */
