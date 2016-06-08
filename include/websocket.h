/** Node type: WebSockets
 *
 * This file implements the websocket type for nodes.
 * It's based on the libwebsockets library.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */
/**
 * @addtogroup websockets WebSockets node type
 * @ingroup node
 * @{
 *********************************************************************************/


#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_

#include "node.h"

/** Internal data per websocket node */
struct websocket {
	struct list connections;	/**< List of active libwebsocket connections (struct websocket_connection) */
	struct list destinations;	/**< List of struct lws_client_connect_info to connect to. */
	
	struct websocket_connection *writer;
};

struct websocket_connection {
	struct node *node;
	struct path *path;
	
	qptr_t received;
};

/** @see node_vtable::init */
int websocket_init(int argc, char * argv[], config_setting_t *cfg);

/** @see node_vtable::deinit */
int websocket_deinit();

/** @see node_vtable::open */
int websocket_open(struct node *n);

/** @see node_vtable::close */
int websocket_close(struct node *n);

/** @see node_vtable::close */
int websocket_destroy(struct node *n);

/** @see node_vtable::read */
int websocket_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_vtable::write */
int websocket_write(struct node *n, struct sample *smps[], unsigned cnt);

#endif /* _WEBSOCKET_H_ */