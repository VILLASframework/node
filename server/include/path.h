/** Message paths
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

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

	/** Timer file descriptor for fixed rate sending */
	int tfd;
	/** Send messages with a fixed rate over this path */
	double rate;
	
	/** Size of the history buffer in number of messages */
	int poolsize;
	/** A circular buffer of past messages */
	struct msg *pool;

	/** A pointer to the last received message */
	struct msg *current;
	/** A pointer to the previously received message */
	struct msg *previous;
	
	/** Counter for received messages according to their sequence no displacement */
	struct hist histogram;

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
 * @param p A pointer to the path structure.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_start(struct path *p);

/** Stop a path.
 *
 * @param p A pointer to the path structure.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_stop(struct path *p);


/** Reset internal counters and histogram of a path.
 *
 * @param p A pointer to the path structure.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_reset(struct path *p);

/** Show some basic statistics for a path.
 *
 * @param p A pointer to the path structure.
 */
void path_print_stats(struct path *p);

/** Fills the provided buffer with a string representation of the path.
 *
 * Format: source => [ dest1 dest2 dest3 ]
 *
 * @param p A pointer to the path structure.
 * @param buf A pointer to the buffer which should be filled.
 * @param len The length of buf in bytes.
 */
int path_print(struct path *p, char *buf, int len);

#endif /* _PATH_H_ */
