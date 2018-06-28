/** Message paths
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

/** A path connects one input node to multiple output nodes (1-to-n).
 *
 * @addtogroup path Path
 * @{
 */

#pragma once

#include <pthread.h>
#include <jansson.h>

#include "list.h"
#include "queue.h"
#include "pool.h"
#include "bitset.h"
#include "common.h"
#include "hook.h"
#include "mapping.h"
#include "task.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct stats;
struct node;
struct super_node;

struct path_source {
	struct node *node;

	bool masked;

	struct pool pool;
	struct list mappings;            /**< List of mappings (struct mapping_entry). */
};

struct path_destination {
	struct node *node;

	struct queue queue;
};

/** The register mode determines under which condition the path is triggered. */
enum path_mode {
	PATH_MODE_ANY,                /**< The path is triggered whenever one of the sources receives samples. */
	PATH_MODE_ALL                /**< The path is triggered only after all sources have received at least 1 sample. */
};

/** The datastructure for a path. */
struct path {
	enum state state;            /**< Path state. */

	enum path_mode mode;        /**< Determines when this path is triggered. */

	struct {
		int nfds;
		struct pollfd *pfds;
	} reader;

	struct pool pool;
	struct sample *last_sample;
	int last_sequence;

	struct list sources;        /**< List of all incoming nodes (struct path_source). */
	struct list destinations;    /**< List of all outgoing nodes (struct path_destination). */
	struct list hooks;        /**< List of processing hooks (struct hook). */

	struct task timeout;

	double rate;            /**< A timeout for */
	int enabled;            /**< Is this path enabled. */
	int poll;            /**< Weather or not to use poll(2). */
	int reverse;            /**< This path as a matching reverse path. */
	int builtin;            /**< This path should use built-in hooks by default. */
	int queuelen;            /**< The queue length for each path_destination::queue */
	int samplelen;            /**< Will be calculated based on path::sources.mappings */

	char *_name;            /**< Singleton: A string which is used to print this path to screen. */

	struct bitset mask;        /**< A mask of path_sources which are enabled for poll(). */
	struct bitset received;        /**< A mask of path_sources for which we already received samples. */

	pthread_t tid;            /**< The thread id for this path. */
	json_t *cfg;            /**< A JSON object containing the configuration of the path. */
};

/** Initialize internal data structures. */
int path_init(struct path *p);

int path_init2(struct path *p);

/** Check if path configuration is proper. */
int path_check(struct path *p);

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

/** Destroy path by freeing dynamically allocated memory.
 *
 * @param i A pointer to the path structure.
 */
int path_destroy(struct path *p);

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
const char *path_name(struct path *p);

/** Reverse a path */
int path_reverse(struct path *p, struct path *r);

/** Check if node is used as source or destination of a path. */
int path_uses_node(struct path *p, struct node *n);

/** Parse a single path and add it to the global configuration.
 *
 * @param cfg A JSON object containing the configuration of the path.
 * @param p Pointer to the allocated memory for this path
 * @param nodes A linked list of all existing nodes
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_parse(struct path *p, json_t *cfg, struct list *nodes);

#ifdef __cplusplus
}
#endif

/** @} */
