/** Nodes
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/node_type.h>
#include <villas/node_direction.h>
#include <villas/sample.h>
#include <villas/list.h>
#include <villas/queue.h>
#include <villas/common.h>

#if defined(LIBNL3_ROUTE_FOUND) && defined(__linux__)
  #define WITH_NETEM
#endif /* LIBNL3_ROUTE_FOUND */

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
#ifdef WITH_NETEM
  struct rtnl_qdisc;
  struct rtnl_cls;
#endif /* WITH_NETEM */

/** The data structure for a node.
 *
 * Every entity which exchanges messages is represented by a node.
 * Nodes can be remote machines and simulators or locally running processes.
 */
struct node {
	char *name;		/**< A short identifier of the node, only used for configuration and logging */

	enum state state;

	char *_name;		/**< Singleton: A string used to print to screen. */
	char *_name_long;	/**< Singleton: A string used to print to screen. */

	int affinity;		/**< CPU Affinity of this node */

	uint64_t sequence;	/**< This is a counter of received samples, in case the node-type does not generate sequence numbers itself. */

	struct stats *stats;	/**< Statistic counters. This is a pointer to the statistic hooks private data. */

	struct node_direction in, out;

#ifdef __linux__
	int fwmark;			/**< Socket mark for netem, routing and filtering */

#ifdef WITH_NETEM
	struct rtnl_qdisc *tc_qdisc;	/**< libnl3: Network emulator queuing discipline */
	struct rtnl_cls *tc_classifier;	/**< libnl3: Firewall mark classifier */
#endif /* WITH_NETEM */
#endif /* __linux__ */

	struct node_type *_vt;	/**< Virtual functions (C++ OOP style) */
	void *_vd;		/**< Virtual data (used by struct node::_vt functions) */

	json_t *cfg;		/**< A JSON object containing the configuration of the node. */
};

/** Initialize node with default values */
int node_init(struct node *n, struct node_type *vt);

/** Do initialization after parsing the configuration */
int node_prepare(struct node *n);

/** Parse settings of a node.
 *
 * @param cfg A JSON object containing the configuration of the node.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int node_parse(struct node *n, json_t *cfg, const char *name);

/** Parse an array or single node and checks if they exist in the "nodes" section.
 *
 * Examples:
 *     out = [ "sintef", "scedu" ]
 *     out = "acs"
 *
 * @param cfg A JSON array or string. See examples above.
 * @param nodes The nodes will be added to this list.
 * @param all This list contains all valid nodes.
 */
int node_list_parse(struct vlist *list, json_t *cfg, struct vlist *all);

/** Parse the list of signal definitions. */
int node_parse_signals(struct vlist *list, json_t *cfg);

/** Validate node configuration. */
int node_check(struct node *n);

/** Start operation of a node.
 *
 * @see node_type::open
 */
int node_start(struct node *n);

/** Stops operation of a node.
 *
 * @see node_type::close
 */
int node_stop(struct node *n);

/** Pauses operation of a node.
 *
 * @see node_type::close
 */
int node_pause(struct node *n);

/** Resumes operation of a node.
 *
 * @see node_type::close
 */
int node_resume(struct node *n);

/** Restarts operation of a node.
 *
 * @see node_type::close
 */
int node_restart(struct node *n);

/** Destroy node by freeing dynamically allocated memory.
 *
 * @see node_type::destroy
 */
int node_destroy(struct node *n);

/** Return a pointer to a string which should be used to print this node.
 *
 * @see node::_name‚
 * @param n A pointer to the node structure.
 */
const char * node_name_short(struct node *n);

/** Return a pointer to a string which should be used to print this node. */
char * node_name(struct node *n);

/** Return a pointer to a string which should be used to print this node.
 *
 * @see node::_name_short
 * @see node_type::print
 * @param n A pointer to the node structure.
 */
char * node_name_long(struct node *n);

/** Reverse local and remote socket address.
 *
 * @see node_type::reverse
 */
int node_reverse(struct node *n);

int node_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

int node_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

int node_poll_fds(struct node *n, int fds[]);

int node_netem_fds(struct node *n, int fds[]);

static inline
struct node_type * node_type(struct node *n)
{
	return n->_vt;
}

struct memory_type * node_memory_type(struct node *n, struct memory_type *parent);

int node_is_valid_name(const char *name);

#ifdef __cplusplus
}
#endif

/** @} */
