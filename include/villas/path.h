/** Message paths
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
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
#include "queue.h"
#include "pool.h"

/** The datastructure for a path.
 *
 * @todo Add support for multiple outgoing nodes
 */
struct path
{
	enum {
		PATH_INVALID,		/**< Path is invalid. */
		PATH_CREATED,		/**< Path has been created. */
		PATH_RUNNING,		/**< Path is currently running. */
		PATH_STOPPED		/**< Path has been stopped. */
	} state;			/**< Path state */
	
	struct node *in;		/**< Pointer to the incoming node */
	
	struct queue queue;		/**< A ring buffer for all received messages (unmodified) */
	struct pool pool;		/**< Memory pool for messages / samples. */

	struct list destinations;	/**< List of all outgoing nodes */
	struct list hooks;		/**< List of function pointers to hooks */

	int samplelen;			/**< Maximum number of values per sample for this path. */
	int queuelen;			/**< Size of sample queue for this path. */
	int enabled;			/**< Is this path enabled */

	pthread_t tid;			/**< The thread id for this path */

	config_setting_t *cfg;		/**< A pointer to the libconfig object which instantiated this path */
	
	char *_name;			/**< Singleton: A string which is used to print this path to screen. */
	
	/** The following fields are mostly managed by hook_ functions @{ */
	
	struct {
		struct hist owd;	/**< Histogram for one-way-delay (OWD) of received messages */
		struct hist gap_msg;	/**< Histogram for inter message timestamps (as sent by remote) */
		struct hist gap_recv;	/**< Histogram for inter message arrival time (as seen by this instance) */
		struct hist gap_seq;	/**< Histogram of sequence number displacement of received messages */
	} hist;

	/* Statistic counters */
	uintmax_t invalid;		/**< Counter for invalid messages */
	uintmax_t skipped;		/**< Counter for skipped messages due to hooks */
	uintmax_t dropped;		/**< Counter for dropped messages due to reordering */
	
	/** @} */
};

/** Create a path by allocating dynamic memory. */
void path_init(struct path *p);

/** Destroy path by freeing dynamically allocated memory.
 *
 * @param i A pointer to the path structure.
 */
void path_destroy(struct path *p);

/** Initialize  pool queue and hooks.
 *
 * Should be called after path_init() and before path_start().
 */
int path_prepare(struct path *p);

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

/** Check if node is used as source or destination of a path. */
int path_uses_node(struct path *p, struct node *n);

#endif /** _PATH_H_ @} */
