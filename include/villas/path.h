/** Message paths
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <bitset>

#include <uuid/uuid.h>
#include <pthread.h>
#include <jansson.h>

#include <villas/list.h>
#include <villas/queue.h>
#include <villas/pool.h>
#include <villas/common.hpp>
#include <villas/mapping.h>
#include <villas/task.hpp>

#include <villas/log.hpp>

/* Forward declarations */
struct vnode;

/** The register mode determines under which condition the path is triggered. */
enum class PathMode {
	ANY,				/**< The path is triggered whenever one of the sources receives samples. */
	ALL				/**< The path is triggered only after all sources have received at least 1 sample. */
};

/** The datastructure for a path. */
struct vpath {
	enum State state;		/**< Path state. */

	enum PathMode mode;		/**< Determines when this path is triggered. */

	uuid_t uuid;

	struct {
		int nfds;
		struct pollfd *pfds;
	} reader;

	struct pool pool;
	struct sample *last_sample;
	int last_sequence;

	struct vlist sources;		/**< List of all incoming nodes (struct vpath_source). */
	struct vlist destinations;	/**< List of all outgoing nodes (struct vpath_destination). */
	struct vlist mappings;		/**< List of all input mappings (struct mapping_entry). */
	struct vlist hooks;		/**< List of processing hooks (struct hook). */
	struct vlist signals;		/**< List of signals which this path creates (struct signal). */

	struct Task timeout;

	double rate;			/**< A timeout for */
	int enabled;			/**< Is this path enabled. */
	int poll;			/**< Weather or not to use poll(2). */
	int reverse;			/**< This path has a matching reverse path. */
	int builtin;			/**< This path should use built-in hooks by default. */
	int original_sequence_no;	/**< Use original source sequence number when multiplexing */
	unsigned queuelen;		/**< The queue length for each path_destination::queue */

	char *_name;			/**< Singleton: A string which is used to print this path to screen. */

	pthread_t tid;			/**< The thread id for this path. */
	json_t *cfg;			/**< A JSON object containing the configuration of the path. */

	villas::Logger logger;

	std::bitset<MAX_SAMPLE_LENGTH> mask;		/**< A mask of path_sources which are enabled for poll(). */
	std::bitset<MAX_SAMPLE_LENGTH> received;	/**< A mask of path_sources for which we already received samples. */
};

/** Initialize internal data structures. */
int path_init(struct vpath *p);

int path_prepare(struct vpath *p);

/** Check if path configuration is proper. */
int path_check(struct vpath *p);

/** Start a path.
 *
 * Start a new pthread for receiving/sending messages over this path.
 *
 * @param p A pointer to the path structure.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_start(struct vpath *p);

/** Stop a path.
 *
 * @param p A pointer to the path structure.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_stop(struct vpath *p);

/** Destroy path by freeing dynamically allocated memory.
 *
 * @param i A pointer to the path structure.
 */
int path_destroy(struct vpath *p);

/** Fills the provided buffer with a string representation of the path.
 *
 * Format: source => [ dest1 dest2 dest3 ]
 *
 * @param p A pointer to the path structure.
 * @return A pointer to a string containing a textual representation of the path.
 */
const char * path_name(struct vpath *p);

/** Get a list of signals which is emitted by the path. */
struct vlist * path_output_signals(struct vpath *n);

/** Reverse a path */
int path_reverse(struct vpath *p, struct vpath *r);

/** Check if node is used as source or destination of a path. */
int path_uses_node(struct vpath *p, struct vnode *n);

/** Parse a single path and add it to the global configuration.
 *
 * @param cfg A JSON object containing the configuration of the path.
 * @param p Pointer to the allocated memory for this path
 * @param nodes A linked list of all existing nodes
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int path_parse(struct vpath *p, json_t *cfg, struct vlist *nodes);

void path_parse_mask(struct vpath *p, json_t *json_mask);

bool path_is_simple(const struct vpath *p);

bool path_is_enabled(const struct vpath *p);

bool path_is_reversed(const struct vpath *p);

struct vlist * path_get_signals(struct vpath *p);

json_t * path_to_json(struct vpath *p);

/** @} */
