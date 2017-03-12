/** Nodes
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *
 * @addtogroup node Node
 * @{
 *********************************************************************************/

#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <libconfig.h>

#include "node_type.h"
#include "sample.h"
#include "list.h"
#include "queue.h"
#include "common.h"

/** The data structure for a node.
 *
 * Every entity which exchanges messages is represented by a node.
 * Nodes can be remote machines and simulators or locally running processes.
 */
struct node
{
	const char *name;	/**< A short identifier of the node, only used for configuration and logging */

	char *_name;		/**< Singleton: A string used to print to screen. */
	char *_name_long;	/**< Singleton: A string used to print to screen. */

	int vectorize;		/**< Number of messages to send / recv at once (scatter / gather) */
	int affinity;		/**< CPU Affinity of this node */

	unsigned long sequence;	/**< This is a counter of received samples, in case the node-type does not generate sequence numbers itself. */
	
	enum state state;
	
	struct node_type *_vt;	/**< Virtual functions (C++ OOP style) */
	void *_vd;		/**< Virtual data (used by struct node::_vt functions) */

	config_setting_t *cfg;	/**< A pointer to the libconfig object which instantiated this node */
};

int node_init(struct node *n, struct node_type *vt);

/** Parse a single node and add it to the global configuration.
 *
 * @param cfg A libconfig object pointing to the node.
 * @param nodes Add new nodes to this linked list.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int node_parse(struct node *n, config_setting_t *cfg);

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

int node_read(struct node *n, struct sample *smps[], unsigned cnt);

int node_write(struct node *n, struct sample *smps[], unsigned cnt);

/** Parse an array or single node and checks if they exist in the "nodes" section.
 *
 * Examples:
 *     out = [ "sintef", "scedu" ]
 *     out = "acs"
 *
 * @param cfg The libconfig object handle for "out".
 * @param nodes The nodes will be added to this list.
 * @param all This list contains all valid nodes.
 */
int node_parse_list(struct list *list, config_setting_t *cfg, struct list *all);

/** @} */