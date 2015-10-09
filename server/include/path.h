/** Message paths
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */
/** A path connects one input node to multiple output nodes (1-to-n).
 *
 * @addtogroup path Path
 * @{
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
	struct list hooks[HOOK_MAX];

	/** Timer file descriptor for fixed rate sending */
	int tfd;
	/** Send messages with a fixed rate over this path */
	double rate;
	
	/** Maximum number of values per message for this path */
	int msgsize;
	/** Size of the history buffer in number of messages */
	int poolsize;
	/** A circular buffer of past messages */
	struct msg *pool;

	/** A pointer to the last received message */
	struct msg *current;
	/** A pointer to the previously received message */
	struct msg *previous;

	/** The thread id for this path */
	pthread_t recv_tid;
	/** A second thread id for fixed rate sending thread */
	pthread_t sent_tid;
	/** A pointer to the libconfig object which instantiated this path */
	config_setting_t *cfg;
	
	/* The following fields are mostly managed by hook_ functions */
	
	/** Histogram of sequence number displacement of received messages */
	struct hist hist_sequence;
	/** Histogram for delay of received messages */
	struct hist hist_delay;
	/** Histogram for inter message delay (time between received messages) */
	struct hist hist_gap;
	
	/** Last message received */
	struct timespec ts_recv;
	/** Last message sent */
	struct timespec ts_sent;

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
 * @return A pointer to a string containing a textual representation of the path.
 */
char * path_print(struct path *p);

/** Conditionally execute the hooks
 *
 * @param p A pointer to the path structure.
 * @param t Which type of hooks should be executed?
 * @retval 0 All registred hooks for the specified type have been executed successfully. 
 * @retval <0 On of the hook functions signalized, that the processing should be aborted; message should be skipped.
 */
int path_run_hook(struct path *p, enum hook_type t);

#endif /** _PATH_H_ @} */
