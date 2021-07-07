/** Node type: WebSockets
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
 * @addtogroup websockets WebSockets node type
 * @ingroup node
 * @{
 */

#pragma once

#include <spdlog/fmt/ostr.h>

#include <villas/pool.h>
#include <villas/queue_signalled.h>
#include <villas/common.hpp>
#include <villas/buffer.hpp>
#include <villas/format.hpp>
#include <villas/node/config.h>

/* Forward declarations */
struct vnode;

#define DEFAULT_WEBSOCKET_QUEUE_LENGTH	(DEFAULT_QUEUE_LENGTH * 64)

/* Forward declaration */
struct lws;

/** Internal data per websocket node */
struct websocket {
	struct vlist destinations;		/**< List of websocket servers connect to in client mode (struct websocket_destination). */

	struct pool pool;
	struct queue_signalled queue;		/**< For samples which are received from WebSockets */
};

struct websocket_destination {
	char *uri;
	struct lws_client_connect_info info;
};

/* Internal datastructures */
struct websocket_connection {
	enum State {
		DESTROYED,
		INITIALIZED,
		CONNECTING,
		RECONNECTING,
		ESTABLISHED,
		SHUTDOWN,
		ERROR
	} state;				/**< The current status of this connection. */

	enum class Mode {
		CLIENT,
		SERVER,
	} mode;

	struct lws *wsi;
	struct vnode *node;
	villas::node::Format *formatter;
	struct queue queue;			/**< For samples which are sent to the Websocket */

	struct websocket_destination *destination;

	struct {
		villas::Buffer *recv;		/**< A buffer for reconstructing fragmented messages. */
		villas::Buffer *send;		/**< A buffer for constructing messages before calling lws_write() */
	} buffers;


	/** Custom formatter for spdlog */
	template<typename OStream>
	friend OStream &operator<<(OStream &os, const struct websocket_connection &c)
	{
		if (c.wsi) {
			char name[128];
			char ip[128];

			lws_get_peer_addresses(c.wsi, lws_get_socket_fd(c.wsi), name, sizeof(name), ip, sizeof(ip));

			os << "remote.ip=" << ip << " remote.name=" << name;
		}
		else if (c.mode == websocket_connection::Mode::CLIENT && c.destination != nullptr)
			os << "dest=" << c.destination->info.address << ":" << c.destination->info.port;

		if (c.node)
			os << ", node=" << *c.node;

		os << ", mode=" << (c.mode == websocket_connection::Mode::CLIENT ? "client" : "server");

		return os;
	}
};

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

/** @see node_type::type_start */
int websocket_type_start(villas::node::SuperNode *sn);

/** @see node_type::type_stop */
int websocket_type_stop();

/** @see node_type::start */
int websocket_start(struct vnode *n);

/** @see node_type::stop */
int websocket_stop(struct vnode *n);

/** @see node_type::stop */
int websocket_destroy(struct vnode *n);

/** @see node_type::read */
int websocket_read(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::write */
int websocket_write(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @} */
