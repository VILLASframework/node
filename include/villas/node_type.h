/** Nodes
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
 *
 * @addtogroup node Node
 * @{
 *********************************************************************************/

#ifdef __cplusplus
extern "C"{
#endif

#pragma once

#include <jansson.h>

#include "list.h"
#include "common.h"

/* Forward declarations */
struct node;
struct super_node;
struct sample;

/** C++ like vtable construct for node_types */
struct node_type {
	int vectorize;			/**< Maximal vector length supported by this node type. Zero is unlimited. */

	struct list instances;		/**< A list of all existing nodes of this type. */
	size_t size;			/**< Size of private data bock. @see node::_vd */

	enum state state;

	/** Global initialization per node type.
	 *
	 * This callback is invoked once per node-type.
	 *
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*init)(struct super_node *sn);

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

	/** Parse node connection details.
	 *
	 * @param n	A pointer to the node object.
	 * @param cfg	A JSON object containing the configuration of the node.
	 * @retval 0 	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*parse)(struct node *n, json_t *cfg);

	/** Check the current node configuration for plausability and errors.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0 	Success. Node configuration is good.
	 * @retval <0	Error. The node configuration is bogus.
	 */
	int (*check)(struct node *n);

	/** Parse node from command line arguments. */
	int (*parse_cli)(struct node *n, int argc, char *argv[]);

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
	int (*start)(struct node *n);

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
	 * Some node-types might only support to receive one message at a time.
	 *
	 * @param n	A pointer to the node object.
	 * @param smps	An array of pointers to memory blocks where the function should store received samples.
	 * @param cnt	The number of messages which should be received.
	 * @return	The number of messages actually received.
	 */
	int (*read)(struct node *n, struct sample *smps[], unsigned cnt);

	/** Send multiple messages in a single datagram / packet.
	 *
	 * Messages are sent with a single sendmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages have to be stored in a circular buffer / array m.
	 * So the indexes will wrap around after len.
	 *
	 * @param n	A pointer to the node object.
	 * @param smps	An array of pointers to memory blocks where samples read from.
	 * @param cnt	The number of messages which should be sent.
	 * @return	The number of messages actually sent.
	 */
	int (*write)(struct node *n, struct sample *smps[], unsigned cnt);

	/** Reverse source and destination of a node.
	 *
	 * This is not supported by all node-types!
	 *
	 * @param n	A pointer to the node object.
	 */
	int (*reverse)(struct node *n);

	/** Return a file descriptor which can be used by poll / select to detect the availability of new data. */
	int (*fd)(struct node *n);
};

/** Initialize all registered node type subsystems.
 *
 * @see node_type::init
 */
int node_type_start(struct node_type *vt, struct super_node *sn);

/** De-initialize node type subsystems.
 *
 * @see node_type::deinit
 */
int node_type_stop(struct node_type *vt);

/** Return a printable representation of the node-type. */
const char * node_type_name(struct node_type *vt);

struct node_type * node_type_lookup(const char *name);

#ifdef __cplusplus
}
#endif

/** @} */
