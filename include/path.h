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

/**
 * @brief The datastructure for a path
 */
struct path
{
	/// Pointers to the incoming and outgoing node
	struct node *in, *out;

	/// Hooks are called for every message which is passed
	int (*hooks[MAX_HOOKS])(struct msg *m);

	/// Counter for received messages
	unsigned int received;

	/// Counter for messages which arrived reordered
	unsigned int delayed;

	/// Counter for messages which arrived multiple times
	unsigned int duplicated;

	/// Last known message number
	unsigned int sequence;

	/// The path thread
	pthread_t tid;
};

/**
 * @brief Setup a new path
 *
 * @param p A pointer to the path structure
 * @param in The node we are receiving messages from
 * @param out The nodes we are sending to
 * @param len count of outgoing nodes
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int path_create(struct path *p, struct node *in, struct node *out);

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
