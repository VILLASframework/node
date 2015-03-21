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

#include "list.h"
#include "config.h"
#include "hist.h"
#include "node.h"
#include "msg.h"
#include "hooks.h"

/** The datastructure for a path.
 *
 * @todo Add support for multiple outgoing nodes
 */
struct path
{
	/** Pointer to the incoming node */
	struct node *in;
	/** Pointer to the first outgoing node.
	 * Usually this is only a pointer to the first list element of path::destinations. */
	struct node *out;
	
	/** List of all outgoing nodes */
	struct list destinations;
	/** List of function pointers to hooks */
	struct list hooks;

	/** Send messages with a fixed rate over this path */
	double rate;

	/** A pointer to the last received message */
	struct msg *last;
	
	/** Counter for received messages according to their sequence no displacement */
	struct hist histogram;

	/** Last known message number */
	unsigned int sequence;

	/** Counter for sent messages to all outgoing nodes */
	unsigned int sent;
	/** Counter for received messages from all incoming nodes */
	unsigned int received;
	/** Counter for invalid messages */
	unsigned int invalid;
	/** Counter for skipped messages due to hooks */
	unsigned int skipped;
	/** Counter for dropped messages due to reordering */
	unsigned int dropped;

	/** The thread id for this path */
	pthread_t recv_tid;
	/** A second thread id for fixed rate sending thread */
	pthread_t sent_tid;
	/** A pointer to the libconfig object which instantiated this path */
	config_setting_t *cfg;
};

/** Create a path by allocating dynamic memory. */
struct path * path_create();

/** Destroy path by freeing dynamically allocated memory.
 *
 * @param i A pointer to the path structure.
 */
void path_destroy(struct path *p);

/** Start a path.
 *
 * Start a new pthread for receiving/sending messages over this path.
 *
 * @param p A pointer to the path struct
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_start(struct path *p);

/** Stop a path.
 *
 * @param p A pointer to the path struct
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_stop(struct path *p);

/** Show some basic statistics for a path.
 *
 * @param p A pointer to the path struct
 */
void path_stats(struct path *p);

int path_print(struct path *p, char *buf, int len);

int path_destroy(struct path *p);

#endif /* _PATH_H_ */
