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

#include <libconfig.h>

#include "list.h"

/* Forward declarations */
struct node;
struct sample;

/** C++ like vtable construct for node_types */
struct node_type {
	int vectorize;			/**< Maximal vector length supported by this node type. Zero is unlimited. */

	struct list instances;		/**< A list of all existing nodes of this type. */
	size_t size;			/**< Size of private data bock. @see node::_vd */
	
	enum node_type_state {
		NODE_TYPE_DEINITIALIZED = 0,
		NODE_TYPE_INITIALIZED
	} state;
	
	/** Global initialization per node type.
	 *
	 * This callback is invoked once per node-type.
	 *
	 * @param argc	Number of arguments passed to the server executable (see main()).
	 * @param argv	Array of arguments  passed to the server executable (see main()).
	 * @param cfg	Root libconfig object of global configuration file.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*init)(int argc, char * argv[], config_setting_t *cfg);

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

	/** Start this node.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*start) (struct node *n);

	/** Stop this node.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*stop)(struct node *n);

	/** Receive multiple messages at once.
	 *
	 * Messages are received with a single recvmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages will be stored in a circular buffer / array @p m.
	 * Indexes used to address @p m will wrap around after len messages.
	 * Some node types might only support to receive one message at a time.
	 *
	 * @param n		A pointer to the node object.
	 * @param smps		An array of pointers to memory blocks where the function should store received samples.
	 * @param cnt		The number of messages which should be received.
	 * @return		The number of messages actually received.
	 */
	int (*read) (struct node *n, struct sample *smps[], unsigned cnt);

	/** Send multiple messages in a single datagram / packet.
	 *
	 * Messages are sent with a single sendmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages have to be stored in a circular buffer / array m.
	 * So the indexes will wrap around after len.
	 *
	 * @param n		A pointer to the node object.
	 * @param smps		An array of pointers to memory blocks where samples read from.
	 * @param cnt		The number of messages which should be sent.
	 * @return		The number of messages actually sent.
	 */
	int (*write)(struct node *n, struct sample *smps[], unsigned cnt);
	
	/** Reverse source and destination of a node.
	 *
	 * This is not supported by all node types!
	 *
	 * @param n		A pointer to the node object.
	 */
	int (*reverse)(struct node *n);
};

/** Initialize all registered node type subsystems.
 *
 * @see node_type::init
 */
int node_type_start(struct node_type *vt, int argc, char *argv[], config_setting_t *cfg);

/** De-initialize node type subsystems.
 *
 * @see node_type::deinit
 */
int node_type_stop(struct node_type *vt);

/** @} */