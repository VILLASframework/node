/** Message paths
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _PATH_H_
#define _PATH_H_

#include <pthread.h>
#include <libconfig.h>

#include "config.h"
#include "node.h"
#include "msg.h"

/** The datastructure for a path
 *
 * @todo Add support for multiple outgoing nodes
 */
struct path
{
	/** Pointer to the incoming node */
	struct node *in;
	/** Pointer to the outgoing node */
	struct node *out;

	/** If non NULL this function is called for every received message.
	 *
	 * This hook can be used to filter messages on a user-defined criteria.
	 * If the function has a non-zero return value the message will be dropped.
	 */
	int (*hook)(struct msg *m);

	/** Send messages with a fixed rate over this path */
	double rate;

	/** A pointer to the last received message */
	struct msg *last;

	/** Counter for received messages */
	unsigned int received;
	/** Counter for messages which arrived reordered */
	unsigned int delayed;
	/** Counter for messages which arrived multiple times */
	unsigned int duplicated;
	/** Last known message number */
	unsigned int sequence;
	/** Counter for received messages with invalid device id or data */
	unsigned int invalid;

	/** The thread id for this path */
	pthread_t tid;
	/** A second thread id for fixed rate sending thread */
	pthread_t tid2;
	/** A pointer to the libconfig object which instantiated this path */
	config_setting_t *cfg;

	/** Linked list pointer */
	struct path *next;
};

/** Start a path.
 *
 * Start a new pthread for receiving/sending messages over this path.
 *
 * @param p A pointer to the path struct
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int path_start(struct path *p);

/** Stop a path.
 *
 * @param p A pointer to the path struct
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int path_stop(struct path *p);

/** Show some basic statistics for a path
 *
 * @param p A pointer to the path struct
 */
void path_stats(struct path *p);

#endif /* _PATH_H_ */
