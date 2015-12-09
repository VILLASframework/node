/** Node type: WebSockets
 *
 * This file implements the websocket type for nodes.
 * It's based on the libwebsockets library.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
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

/* Forward declarations */
struct msg;
struct libwebsocket_context;

struct websocket {	
	struct {
		pthread_cond_t cond;
		pthread_mutex_t mutex;
		struct msg *m;
	} read, write;
	
	struct list connections; /**< List of struct libwebsockets sockets */
};

/** @see node_vtable::init */
int websocket_init(int argc, char * argv[], struct settings *set);

/** @see node_vtable::deinit */
int websocket_deinit();

/** @see node_vtable::open */
int websocket_open(struct node *n);

/** @see node_vtable::close */
int websocket_close(struct node *n);

/** @see node_vtable::read */
int websocket_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

/** @see node_vtable::write */
int websocket_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

#endif /* _WEBSOCKET_H_ */