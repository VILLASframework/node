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
#define node_type(n)			((n)->_vt->type)
#define node_print(n)			((n)->_vt->print(n))

#define node_read(n, p, ps, f, c)	((n)->_vt->read(n, p, ps, f, c))
#define node_write(n, p, ps, f, c)	((n)->_vt->write(n, p, ps, f, c))

#define node_read_single(n, m)		((n)->_vt->read(n, m, 1, 0, 1))
#define node_write_single(n, m)		((n)->_vt->write(n, m, 1, 0, 1))


#define REGISTER_NODE_TYPE(type, name, fnc)				\
__attribute__((constructor)) void __register_node_ ## fnc () {		\
	static struct node_type t = { name, type,			\
					fnc ## _parse, fnc ## _print,	\
					fnc ## _open,  fnc ## _close,	\
					fnc ## _read,  fnc ## _write,	\
					fnc ## _init,  fnc ## _deinit };\
	list_push(&node_types, &t);					\
}

extern struct list node_types;

/** C++ like vtable construct for node_types */
struct node_type {
	/** The unique name of this node. This must be allways the first member! */
	char *name;
	
	/** Node type */
	enum {
		BSD_SOCKET,		/**< BSD Socket API */
		LOG_FILE,		/**< File IO */
		OPAL_ASYNC,		/**< OPAL-RT Asynchronous Process Api */
		GTFPGA,			/**< Xilinx ML507 GTFPGA card */
		NGSI			/**< NGSI 9/10 HTTP RESTful API (FIRWARE ContextBroker) */
	} type;

	/** Parse node connection details.‚
	 *
	 * @param cfg	A libconfig object pointing to the node.
	 * @param n	A pointer to the node structure which should be parsed.
	 * @retval 0 	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*parse)(config_setting_t *cfg, struct node *n);

	/** Returns a string with a textual represenation of this node.
	 *
	 * @param n	A pointer to the node structure
	 * @return	A pointer to a dynamically allocated string. Must be freed().
	 */
	char * (*print)(struct node *n);

	/** Opens the connection to this node.
	 *
	 * @param n	A pointer to the node.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*open) (struct node *n);

	/** Close the connection to this node.
	 *
	 * @param n	A pointer to the node.
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
	 * @param n 		A pointer to the node where the messages should be sent to.
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
	 * @param n		A pointer to the node where the messages should be sent to.
	 * @param pool		A pointer to an array of messages which should be sent.
	 * @param poolsize	The length of the message array.
	 * @param first		The index of the first message which should be sent.
	 * @param cnt		The number of messages which should be sent.
	 * @return		The number of messages actually sent.
	 */
	int (*write)(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

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

	int refcnt;	/**< Reference counter: how many nodes are using this node-type? */
};

/** The data structure for a node.
 *
 * Every entity which exchanges messages is represented by a node.
 * Nodes can be remote machines and simulators or locally running processes.
 */
struct node
{
	char *name;		/**< A short identifier of the node, only used for configuration and logging */
	int refcnt;		/**< How many paths  are sending / receiving from this node? */
	int combine;		/**< Number of messages to send / recv at once (scatter / gather) */
	int affinity;		/**< CPU Affinity of this node */

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

/** Initialize node type subsystems.
 *
 * These routines are only called once per type (not per node instance).
 * See node_vtable::init
 */
int node_init(int argc, char *argv[], struct settings *set);

/** De-initialize node type subsystems.
 *
 * These routines are only called once per type (not per node instance).
 * See node_vtable::deinit
 */
int node_deinit();

/** Connect and bind the UDP socket of this node.
 *
 * Depending on the type (vtable) of this node,
 *  a socket is opened, shmem region registred...
 *
 * @param n A pointer to the node structure
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int node_start(struct node *n);

/** Deferred TCP connection setup
 *
 * @todo Dirty hack!
 *  We should check the address of the connecting node!
 *  We should preserve the original socket for proper shutdown.
 */
int node_start_defer(struct node *n);

/** Stops a node.
 *
 * Depending on the type (vtable) of this node,
 *  a socket is closed, shmem region released...
 *
 * @param n A pointer to the node structure
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int node_stop(struct node *n);

/** Reverse local and remote socket address.
 * This is usefull for the helper programs: send, receive, test
 * because they usually use the same configuration file as the
 * server and therefore the direction needs to be swapped. */
void node_reverse(struct node *n);

/** Create a node by allocating dynamic memory. */
struct node * node_create();

/** Destroy node by freeing dynamically allocated memory.
 *
 * @param i A pointer to the interface structure.
 */
void node_destroy(struct node *n);

#endif /** _NODE_H_ @} */
