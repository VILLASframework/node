/** Nodes
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

/**
 * @addtogroup node Node
 * @{
 */

#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <jansson.h>
#include <uuid/uuid.h>

#include <villas/node_type.h>
#include <villas/node_direction.h>
#include <villas/sample.h>
#include <villas/list.h>
#include <villas/queue.h>
#include <villas/common.hpp>
#include <villas/stats.hpp>
#include <villas/log.hpp>

#if defined(LIBNL3_ROUTE_FOUND) && defined(__linux__)
  #define WITH_NETEM
#endif /* LIBNL3_ROUTE_FOUND */

/* Forward declarations */
#ifdef WITH_NETEM
  struct rtnl_qdisc;
  struct rtnl_cls;
#endif /* WITH_NETEM */

#define RE_NODE_NAME "[a-z0-9_-]{2,32}"

/** The data structure for a node.
 *
 * Every entity which exchanges messages is represented by a node.
 * Nodes can be remote machines and simulators or locally running processes.
 */
struct vnode {
	char *name;		/**< A short identifier of the node, only used for configuration and logging */
	bool enabled;

	enum State state;
	villas::Logger logger;

	char *_name;		/**< Singleton: A string used to print to screen. */
	char *_name_long;	/**< Singleton: A string used to print to screen. */

	uuid_t uuid;

	int affinity;		/**< CPU Affinity of this node */

	uint64_t sequence;	/**< This is a counter of received samples, in case the node-type does not generate sequence numbers itself. */

	std::shared_ptr<villas::Stats> stats;		/**< Statistic counters. This is a pointer to the statistic hooks private data. */

	struct vnode_direction in, out;

#ifdef __linux__
	int fwmark;			/**< Socket mark for netem, routing and filtering */

#ifdef WITH_NETEM
	struct rtnl_qdisc *tc_qdisc;	/**< libnl3: Network emulator queuing discipline */
	struct rtnl_cls *tc_classifier;	/**< libnl3: Firewall mark classifier */
#endif /* WITH_NETEM */
#endif /* __linux__ */

	struct vlist sources;		/**< A list of path sources which reference this node (struct vpath_sources). */
	struct vlist destinations;	/**< A list of path destinations which reference this node (struct vpath_destinations). */

	struct vnode_type *_vt;	/**< Virtual functions (C++ OOP style) */
	void *_vd;		/**< Virtual data (used by struct vnode::_vt functions) */

	json_t *config;		/**< A JSON object containing the configuration of the node. */
};

/** Initialize node with default values */
int node_init(struct vnode *n, struct vnode_type *vt) __attribute__ ((warn_unused_result));

/** Do initialization after parsing the configuration */
int node_prepare(struct vnode *n);

/** Parse settings of a node.
 *
 * @param json A JSON object containing the configuration of the node.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int node_parse(struct vnode *n, json_t *json, const uuid_t sn_uuid);

/** Parse an array or single node and checks if they exist in the "nodes" section.
 *
 * Examples:
 *     out = [ "sintef", "scedu" ]
 *     out = "acs"
 *
 * @param json A JSON array or string. See examples above.
 * @param nodes The nodes will be added to this list.
 * @param all This list contains all valid nodes.
 */
int node_list_parse(struct vlist *list, json_t *json, struct vlist *all);

/** Parse the list of signal definitions. */
int node_parse_signals(struct vlist *list, json_t *json);

/** Validate node configuration. */
int node_check(struct vnode *n);

/** Start operation of a node.
 *
 * @see node_type::start
 */
int node_start(struct vnode *n);

/** Stops operation of a node.
 *
 * @see node_type::stop
 */
int node_stop(struct vnode *n);

/** Pauses operation of a node.
 *
 * @see node_type::stop
 */
int node_pause(struct vnode *n);

/** Resumes operation of a node.
 *
 * @see node_type::stop
 */
int node_resume(struct vnode *n);

/** Restarts operation of a node.
 *
 * @see node_type::stop
 */
int node_restart(struct vnode *n);

/** Destroy node by freeing dynamically allocated memory.
 *
 * @see node_type::destroy
 */
int node_destroy(struct vnode *n) __attribute__ ((warn_unused_result));

/** Return a pointer to a string which should be used to print this node.
 *
 * @see node::_name‚
 * @param n A pointer to the node structure.
 */
const char * node_name_short(struct vnode *n);

/** Return a pointer to a string which should be used to print this node. */
char * node_name(struct vnode *n);

/** Return a pointer to a string which should be used to print this node.
 *
 * @see node::_name_short
 * @see node_type::print
 * @param n A pointer to the node structure.
 */
char * node_name_long(struct vnode *n);

/** Return a list of signals which are sent to this node.
 *
 * This list is derived from the path which uses the node as destination.
 */
struct vlist * node_output_signals(struct vnode *n);

struct vlist * node_input_signals(struct vnode *n);

/** Reverse local and remote socket address.
 *
 * @see node_type::reverse
 */
int node_reverse(struct vnode *n);

int node_read(struct vnode *n, struct sample * smps[], unsigned cnt);

int node_write(struct vnode *n, struct sample * smps[], unsigned cnt);

int node_poll_fds(struct vnode *n, int fds[]);

int node_netem_fds(struct vnode *n, int fds[]);

static inline
struct vnode_type * node_type(struct vnode *n)
{
	return n->_vt;
}

struct memory_type * node_memory_type(struct vnode *n);

bool node_is_valid_name(const char *name);

bool node_is_enabled(const struct vnode *n);

json_t * node_to_json(struct vnode *);

/** @} */
