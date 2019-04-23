/** Node type: WebSockets
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
 * @addtogroup websockets WebSockets node type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node.h>
#include <villas/pool.h>
#include <villas/queue_signalled.h>
#include <villas/common.h>
#include <villas/buffer.h>
#include <villas/io.h>
#include <villas/node/config.h>

#define DEFAULT_WEBSOCKET_QUEUE_LENGTH	(DEFAULT_QUEUE_LENGTH * 64)
#define DEFAULT_WEBSOCKET_SAMPLE_LENGTH	DEFAULT_SAMPLE_LENGTH

/* Forward declaration */
struct lws;

/** Internal data per websocket node */
struct websocket {
	struct vlist destinations;		/**< List of websocket servers connect to in client mode (struct websocket_destination). */

	struct pool pool;
	struct queue_signalled queue;		/**< For samples which are received from WebSockets */
};

/* Internal datastructures */
struct websocket_connection {
	enum state {
		DESTROYED,
		INITIALIZED,
		CONNECTING,
		RECONNECTING,
		ESTABLISHED,
		SHUTDOWN,
		ERROR
	} state;				/**< The current status of this connection. */

	enum mode {
		CLIENT,
		SERVER,
	} mode;

	struct lws *wsi;
	struct node *node;
	struct io io;
	struct queue queue;			/**< For samples which are sent to the WebSocket */

	struct format_type *format;
	struct websocket_destination *destination;

	struct {
		struct buffer recv;		/**< A buffer for reconstructing fragmented messags. */
		struct buffer send;		/**< A buffer for contsructing messages before calling lws_write() */
	} buffers;

	char *_name;
};

struct websocket_destination {
	char *uri;
	struct lws_client_connect_info info;
};

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

/** @see node_type::type_start */
int websocket_type_start(villas::node::SuperNode *sn);

/** @see node_type::type_stop */
int websocket_type_stop();

/** @see node_type::open */
int websocket_start(struct node *n);

/** @see node_type::close */
int websocket_stop(struct node *n);

/** @see node_type::close */
int websocket_destroy(struct node *n);

/** @see node_type::read */
int websocket_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::write */
int websocket_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @} */
