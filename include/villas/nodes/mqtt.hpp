/** Node type: mqtt
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

#include <villas/pool.hpp>
#include <villas/format.hpp>
#include <villas/queue_signalled.h>
#include <villas/super_node.hpp>

/* Forward declarations */
struct mosquitto;

namespace villas {
namespace node {

/* Forward declarations */
class NodeCompat;

struct mqtt {
	struct mosquitto *client;
	struct CQueueSignalled queue;
	struct Pool pool;

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
		int enabled;	   /**< Enable SSL encrypted connection to broker. */
		int insecure;	   /**< Allow insecure SSL connections. */
		char *cafile;	   /**< SSL CA file. */
		char *capath;	   /**< SSL CA path. */
		char *certfile;	   /**< SSL certificate. */
		char *keyfile;	   /**< SSL private key. */
		int cert_reqs;	   /**< SSL_VERIFY_NONE(0) or SSL_VERIFY_PEER(1) */
		char *tls_version; /**< SSL tls verion */
		char *ciphers;      /**< SSL chipher list. */

	} ssl;

	Format *formatter;
};

int mqtt_reverse(NodeCompat *n);

char * mqtt_print(NodeCompat *n);

int mqtt_prepare(NodeCompat *n);

int mqtt_parse(NodeCompat *n, json_t *json);

int mqtt_check(NodeCompat *n);

int mqtt_start(NodeCompat *n);

int mqtt_init(NodeCompat *n);

int mqtt_destroy(NodeCompat *n);

int mqtt_stop(NodeCompat *n);

int mqtt_type_start(SuperNode *sn);

int mqtt_type_stop();

int mqtt_poll_fds(NodeCompat *n, int fds[]);

int mqtt_read(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

int mqtt_write(NodeCompat *n, struct Sample * const smps[], unsigned cnt);

} /* namespace node */
} /* namespace villas */
