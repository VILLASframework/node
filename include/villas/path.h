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
#include "stats.h"

/* Forward declarations */
typedef struct config_setting_t config_setting_t;

struct path_source
{
	struct node *node;
	struct pool pool;
	int samplelen;
	pthread_t tid;	
};

struct path_destination
{
	struct node *node;
	struct queue queue;
	int queuelen;
	pthread_t tid;
};

/** The datastructure for a path. */
struct path
{
	enum {
		PATH_INVALID,		/**< Path is invalid. */
		PATH_INITIALIZED,	/**< Path queues, memory pools & hook system initialized. */
		PATH_RUNNING,		/**< Path is currently running. */
		PATH_STOPPED,		/**< Path has been stopped. */
		PATH_DESTROYED		/**< Path is destroyed. */
	} state;			/**< Path state */
	
	/* Each path has a single source and multiple destinations */
	struct path_source *source;	/**< Pointer to the incoming node */
	struct list destinations;	/**< List of all outgoing nodes (struct path_destination). */

	struct list hooks;		/**< List of function pointers to hooks */

	int enabled;			/**< Is this path enabled */
	int reverse;			/**< This path as a matching reverse path */

	pthread_t tid;			/**< The thread id for this path */
	
	char *_name;			/**< Singleton: A string which is used to print this path to screen. */
	
	struct stats *stats;		/**< Statistic counters. This is a pointer to the statistic hooks private data. */
	
	config_setting_t *cfg;		/**< A pointer to the libconfig object which instantiated this path */
};

/** Allocate memory for a new path */
struct path * path_create();

/** Initialize internal data structures. */
int path_init(struct path *p);

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

/** Reverse a path */
int path_reverse(struct path *p, struct path *r);

/** Check if node is used as source or destination of a path. */
int path_uses_node(struct path *p, struct node *n);

/** Parse a single path and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the path
 * @param p Pointer to the allocated memory for this path
 * @param nodes A linked list of all existing nodes
 * @param set The global configuration structure
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_parse(struct path *p, config_setting_t *cfg, struct list *nodes, struct settings *set);

#endif /** _PATH_H_ @} */
