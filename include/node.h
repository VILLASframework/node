/** Nodes
 *
 * The S2SS server connects multiple nodes.
 * There are multiple types of nodes:
 *  - simulators
 *  - servers
 *  - workstations
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *
 * @addtogroup node Node
 * @{
 *********************************************************************************/

#ifndef _NODE_H_
#define _NODE_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <libconfig.h>

#include "msg.h"
#include "list.h"

/* Helper macros for virtual node type */
#define REGISTER_NODE_TYPE(vt)				\
__attribute__((constructor)) static void __register() {	\
	list_push(&node_types, vt);			\
}

extern struct list node_types;

/** C++ like vtable construct for node_types */
struct node_type {
	/** The unique name of this node. This must be allways the first member! */
	const char *name;
	/** A short description of this node type. Will be shown in help text. */
	const char *description;
	/** A list of all existing nodes of this type. */
	struct list instances;
	
	/** Global initialization per node type.
	 *
	 * This callback is invoked once per node-type.
	 *
	 * @param argc	Number of arguments passed to the server executable (see main()).
	 * @param argv	Array of arguments  passed to the server executable (see main()).
	 * @param set	Global settings.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*init)(int argc, char *argv[], struct settings *set);

	/** Global de-initialization per node type.
	 *
	 * This callback is invoked once per node-type.
	 *
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*deinit)();
	
	/** Allocate memory for an instance of this type. 
	 *
	 * @return A pointer to the node-type specific private data.
	 */
	void * (*create)();
	
	/** Free memory of an instance of this type.
	 *
	 * @param n	A pointer to the node object.
	 */
	int (*destroy)(struct node *n);

	/** Parse node connection details.‚
	 *
	 * @param n	A pointer to the node object.
	 * @param cfg	A libconfig object pointing to the node.
	 * @retval 0 	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*parse)(struct node *n, config_setting_t *cfg);

	/** Returns a string with a textual represenation of this node.
	 *
	 * @param n	A pointer to the node object.
	 * @return	A pointer to a dynamically allocated string. Must be freed().
	 */
	char * (*print)(struct node *n);

	/** Opens the connection to this node.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*open) (struct node *n);

	/** Close the connection to this node.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*close)(struct node *n);

	/** Receive multiple messages at once.
	 *
	 * Messages are received with a single recvmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages will be stored in a circular buffer / array @p m.
	 * Indexes used to address @p m will wrap around after len messages.
	 * Some node types might only support to receive one message at a time.
	 *
	 * @param n		A pointer to the node object.
	 * @param pool 		A pointer to an array of messages which should be sent.
	 * @param poolsize 	The length of the message array.
	 * @param first		The index of the first message which should be sent.
	 * @param cnt		The number of messages which should be sent.
	 * @return		The number of messages actually received.
	 */
	int (*read) (struct node *n, struct msg *pool, int poolsize, int first, int cnt);

	/** Send multiple messages in a single datagram / packet.
	 *
	 * Messages are sent with a single sendmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages have to be stored in a circular buffer / array m.
	 * So the indexes will wrap around after len.
	 *
	 * @param n		A pointer to the node object.
	 * @param pool		A pointer to an array of messages which should be sent.
	 * @param poolsize	The length of the message array.
	 * @param first		The index of the first message which should be sent.
	 * @param cnt		The number of messages which should be sent.
	 * @return		The number of messages actually sent.
	 */
	int (*write)(struct node *n, struct msg *pool, int poolsize, int first, int cnt);
	
	/** Reverse source and destination of a node.
	 *
	 * This is not supported by all node types!
	 *
	 * @param n		A pointer to the node object.
	 */
	int (*reverse)(struct node *n);
};

/** The data structure for a node.
 *
 * Every entity which exchanges messages is represented by a node.
 * Nodes can be remote machines and simulators or locally running processes.
 */
struct node
{
	const char *name;	/**< A short identifier of the node, only used for configuration and logging */
	char *_print;		/**< A string used to print to screen. */

	int combine;		/**< Number of messages to send / recv at once (scatter / gather) */
	int affinity;		/**< CPU Affinity of this node */

	enum node_state {
		NODE_INVALID,	/**< This node object is not in a valid state. */
		NODE_CREATED,	/**< This node has been parsed from the configuration. */
		NODE_RUNNING,	/**< This node has been started by calling node_open() */
		NODE_STOPPED	/**< Node was running, but has been stopped by calling node_close() */
	} state;		/**< Node state */

	struct node_type *_vt;	/**< C++ like virtual function call table */

	union {
		struct socket *socket;
		struct opal   *opal;
		struct gtfpga *gtfpga;
		struct file   *file;
		struct ngsi   *ngsi;
	};	/** Virtual data (used by struct node::_vt functions) */

	config_setting_t *cfg;	/**< A pointer to the libconfig object which instantiated this node */
};

/** Initialize all registered node type subsystems.
 *
 * @see node_type::init
 */
int node_init(int argc, char *argv[], struct settings *set);

/** De-initialize node type subsystems.
 *
 * @see node_type::deinit
 */
int node_deinit();

/** Create a node by allocating dynamic memory.
 *
 * @see node_type::create
 * @param vt A pointer to the node-type table of virtual functions.
 */
struct node * node_create(struct node_type *vt);

/** Destroy node by freeing dynamically allocated memory.
 *
 * @see node_type::destroy
 */
void node_destroy(struct node *n);

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

/** Parse node connection details.
 *
 * @see node_type::parse
 */
int node_parse(struct node *n, config_setting_t *cfg);

/** Return a pointer to a string which should be used to print this node
 *
 * @see node_type::print
 * @param i A pointer to the interface structure.
 */
char * node_print(struct node *n);

/** Receive multiple messages at once.
 *
 * @see node_type::read
 */
int node_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

/** Send multiple messages in a single datagram / packet.
 *
 * @see node_type::write
 */
int node_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

/** Reverse local and remote socket address.
 *
 * @see node_type::reverse
 */
int node_reverse(struct node *n);

#endif /** _NODE_H_ @} */
