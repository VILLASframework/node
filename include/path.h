/**
 * Message paths
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _PATH_H_
#define _PATH_H_

#include <pthread.h>

#include "config.h"
#include "node.h"
#include "msg.h"

enum path_state
{
	UNKNOWN,
	CONNECTED,
	RUNNING,
	STOPPED
};

/**
 * @brief The datastructure for a path
 */
struct path
{
	struct node *in;
	struct node *out[MAX_NODES];

	/// Hooks are called for every message which is passed
	int (*hooks[MAX_HOOKS])(struct msg *m);

	/// Counter for received messages
	int msg_received;

	/// Counter for dropped messages
	int msg_dropped;

	/// Last known message number
	int seq_no;

	/// The current path state
	enum path_state state;

	/// The path thread
	pthread_t tid;
};

/**
 * @brief Create a new path
 *
 * Memory is allocated dynamically and has to be freed by path_destroy()
 *
 * @param in The node we are receiving messages from
 * @param out The nodes we are sending to
 * @param len count of outgoing nodes
 * @return
 *  - a pointer to the new path on success
 *  - NULL if an error occured
 */
struct path* path_create(struct node *in, struct node *out[], int len);

/**
 * @brief Delete a path created by path_create()
 *
 * @param p A pointer to the path struct
 */
void path_destroy(struct path *p);

/**
 * @brief Start a path
 *
 * @param p A pointer to the path struct
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int path_start(struct path *p);

/**
 * @brief Stop a path
 *
 * @param p A pointer to the path struct
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int path_stop(struct path *p);

#endif /* _PATH_H_ */
