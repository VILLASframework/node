/** Node type: WebSockets
 *
 * This file implements the websocket type for nodes.
 * It's based on the libwebsockets library.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/**
 * @addtogroup websockets WebSockets node type
 * @ingroup node
 * @{
 */

#pragma once

#include <libwebsockets.h>

#include "node.h"
#include "pool.h"
#include "queue.h"
#include "common.h"

/* Forward declaration */
struct lws;

/** Internal data per websocket node */
struct websocket {
	struct list connections;		/**< List of active libwebsocket connections in server mode (struct websocket_connection). */
	struct list destinations;		/**< List of websocket servers connect to in client mode (struct websocket_destination). */
	
	struct pool pool;
	struct queue queue;			/**< For samples which are received from WebSockets a */
	
};

/* Internal datastructures */
struct websocket_connection {
	struct node *node;
	struct lws *wsi;
	
	struct queue queue;			/**< For samples which are sent to the WebSocket */	
	
	struct {
		char name[64];
		char ip[64];
	} peer;
	
	enum state state;
	
	char *_name;
};

/* Internal datastructures */
struct websocket_destination {
	char *uri;
	struct lws_client_connect_info info;
};

int websocket_protocol_cb(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

/** @see node_vtable::init */
int websocket_init(int argc, char *argv[], config_setting_t *cfg);

/** @see node_vtable::deinit */
int websocket_deinit();

/** @see node_vtable::open */
int websocket_start(struct node *n);

/** @see node_vtable::close */
int websocket_stop(struct node *n);

/** @see node_vtable::close */
int websocket_destroy(struct node *n);

/** @see node_vtable::read */
int websocket_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_vtable::write */
int websocket_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @} */