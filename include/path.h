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
	enum {
		PATH_INVALID,		/**< */
		PATH_CREATED,		/**< */
		PATH_RUNNING,		/**< */
		PATH_STOPPED		/**< */
	} state;			/**< Path state */
	
	struct node *in;		/**< Pointer to the incoming node */
	struct node *out;		/**< Pointer to the first outgoing node ( path::out == list_first(path::destinations) */
	
	struct list destinations;	/**< List of all outgoing nodes */
	struct list hooks;		/**< List of function pointers to hooks */

	int enabled;			/**< Is this path enabled */
	int tfd;			/**< Timer file descriptor for fixed rate sending */
	double rate;			/**< Send messages with a fixed rate over this path */
	
	int msgsize;			/**< Maximum number of values per message for this path */
	int poolsize;			/**< Size of the history buffer in number of messages */
	
	struct msg *pool;		/**< A circular buffer of past messages */

	struct msg *current;		/**< A pointer to the last received message */
	struct msg *previous;		/**< A pointer to the previously received message */

	pthread_t recv_tid;		/**< The thread id for this path */
	pthread_t sent_tid;		/**< A second thread id for fixed rate sending thread */

	config_setting_t *cfg;		/**< A pointer to the libconfig object which instantiated this path */
	
	char *_name;			/**< Singleton: A string which is used to print this path to screen. */
	
	/** The following fields are mostly managed by hook_ functions @{ */
	
	struct {
		struct hist owd;	/**< Histogram for one-way-delay (OWD) of received messages */
		struct hist gap_msg;	/**< Histogram for inter message timestamps (as sent by remote) */
		struct hist gap_recv;	/**< Histogram for inter message arrival time (as seen by this instance) */
		struct hist gap_seq;	/**< Histogram of sequence number displacement of received messages */
	} hist;

	struct {
		struct timespec recv;	/**< Last message received */
		struct timespec sent;	/**< Last message sent */
		struct timespec last;	/**< Previous message received (old value of path::ts__recv) */
	} ts;

	unsigned int sent;		/**< Counter for sent messages to all outgoing nodes */
	unsigned int received;		/**< Counter for received messages from all incoming nodes */
	unsigned int invalid;		/**< Counter for invalid messages */
	unsigned int skipped;		/**< Counter for skipped messages due to hooks */
	unsigned int dropped;		/**< Counter for dropped messages due to reordering */
	unsigned int overrun;		/**< Counter of overruns for fixed-rate sending */
	
	/** @} */
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
const char * path_name(struct path *p);

/** Conditionally execute the hooks
 *
 * @param p A pointer to the path structure.
 * @param t Which type of hooks should be executed?
 * @retval 0 All registred hooks for the specified type have been executed successfully. 
 * @retval <0 On of the hook functions signalized, that the processing should be aborted; message should be skipped.
 */
int path_run_hook(struct path *p, enum hook_type t);

#endif /** _PATH_H_ @} */
