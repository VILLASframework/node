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
 *
 * @addtogroup node Node
 * @{
 *********************************************************************************/

#pragma once

#include <jansson.h>

#include <villas/list.h>
#include <villas/common.hpp>
#include <villas/memory.h>

/* Forward declarations */
struct vnode;
struct sample;

namespace villas {
namespace node {
class SuperNode;
}
}

enum class NodeFlags {
	PROVIDES_SIGNALS	= (1 << 0)
};

/** C++ like vtable construct for node_types */
struct vnode_type {
	unsigned vectorize;		/**< Maximal vector length supported by this node type. Zero is unlimited. */
	int flags;

	enum State state;		/**< State of this node-type. */

	struct vlist instances;		/**< A list of all existing nodes of this type. */

	size_t size;			/**< Size of private data bock. @see node::_vd */
	size_t pool_size;

	struct {
		/** Global initialization per node type.
		 *
		 * This callback is invoked once per node-type.
		 * This callback is optional. It will only be called if non-null.
		 *
		 * @retval 0	Success. Everything went well.
		 * @retval <0	Error. Something went wrong.
		 */
		int (*start)(villas::node::SuperNode *sn);

		/** Global de-initialization per node type.
		 *
		 * This callback is invoked once per node-type.
		 * This callback is optional. It will only be called if non-null.
		 *
		 * @retval 0	Success. Everything went well.
		 * @retval <0	Error. Something went wrong.
		 */
		int (*stop)();
	} type;

	/** Initialize a new node instance.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*init)(struct vnode *n);

	/** Free memory of an instance of this type.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 */
	int (*destroy)(struct vnode *n);

	/** Parse node connection details.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @param cfg	A JSON object containing the configuration of the node.
	 * @retval 0 	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*parse)(struct vnode *n, json_t *cfg);

	/** Check the current node configuration for plausability and errors.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0 	Success. Node configuration is good.
	 * @retval <0	Error. The node configuration is bogus.
	 */
	int (*check)(struct vnode *n);

	int (*prepare)(struct vnode *);

	/** Returns a string with a textual represenation of this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @return	A pointer to a dynamically allocated string. Must be freed().
	 */
	char * (*print)(struct vnode *n);

	/** Start this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*start)(struct vnode *n);

	/** Restart this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*restart)(struct vnode *n);

	/** Stop this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*stop)(struct vnode *n);

	/** Pause this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*pause)(struct vnode *n);

	/** Resume this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*resume)(struct vnode *n);

	/** Receive multiple messages at once.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * Messages are received with a single recvmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages will be stored in a circular buffer / array @p m.
	 * Indexes used to address @p m will wrap around after len messages.
	 * Some node-types might only support to receive one message at a time.
	 *
	 * @param n		    A pointer to the node object.
	 * @param smps		An array of pointers to memory blocks where the function should store received samples.
	 * @param cnt		The number of samples that are allocated by the calling function.
	 * @param release	The number of samples that should be released after read is called.
	 * @return		    The number of messages actually received.
	 */
	int (*read)(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release);

	/** Send multiple messages in a single datagram / packet.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * Messages are sent with a single sendmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages have to be stored in a circular buffer / array m.
	 * So the indexes will wrap around after len.
	 *
	 * @param n		    A pointer to the node object.
	 * @param smps		An array of pointers to memory blocks where samples read from.
	 * @param cnt		The number of samples that are allocated by the calling function.
	 * @param release	The number of samples that should be released after write is called
	 * @return		    The number of messages actually sent.
	 */
	int (*write)(struct vnode *n, struct sample *smps[], unsigned cnt, unsigned *release);

	/** Reverse source and destination of a node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 */
	int (*reverse)(struct vnode *n);

	/** Get list of file descriptors which can be used by poll/select to detect the availability of new data.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @return The number of file descriptors which have been put into \p fds.
	 */
	int (*poll_fds)(struct vnode *n, int fds[]);

	/** Get list of socket file descriptors for configuring network emulation.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @return The number of file descriptors which have been put into \p sds.
	 */
	int (*netem_fds)(struct vnode *n, int sds[]);

	/** Return a memory allocator which should be used for sample pools passed to this node. */
	struct memory_type * (*memory_type)(struct vnode *n, struct memory_type *parent);
};

/** Initialize all registered node type subsystems.
 *
 * @see node_type::init
 */
int node_type_start(struct vnode_type *vt, villas::node::SuperNode *sn);

/** De-initialize node type subsystems.
 *
 * @see node_type::deinit
 */
int node_type_stop(struct vnode_type *vt);

/** Return a printable representation of the node-type. */
const char * node_type_name(struct vnode_type *vt);

struct vnode_type * node_type_lookup(const char *name);

/** @} */
