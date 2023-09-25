/* Nodes.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <jansson.h>

#include <villas/common.hpp>
#include <villas/log.hpp>
#include <villas/node/memory.hpp>

namespace villas {
namespace node {

// Forward declarations
struct Sample;
class Node;
class SuperNode;

class NodeCompatType {

public:
  const char *name;
  const char *description;

  unsigned
      vectorize; // Maximal vector length supported by this node type. Zero is unlimited.
  int flags;

  enum State state; // State of this node-type.

  size_t size; // Size of private data bock. @see node::_vd

  struct {
    /* Global initialization per node type.
		 *
		 * This callback is invoked once per node-type.
		 * This callback is optional. It will only be called if non-null.
		 *
		 * @retval 0	Success. Everything went well.
		 * @retval <0	Error. Something went wrong.
		 */
    int (*start)(SuperNode *sn);

    /* Global de-initialization per node type.
		 *
		 * This callback is invoked once per node-type.
		 * This callback is optional. It will only be called if non-null.
		 *
		 * @retval 0	Success. Everything went well.
		 * @retval <0	Error. Something went wrong.
		 */
    int (*stop)();
  } type;

  /* Initialize a new node instance.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
  int (*init)(NodeCompat *n);

  /* Free memory of an instance of this type.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 */
  int (*destroy)(NodeCompat *n);

  /* Parse node connection details.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @param json	A JSON object containing the configuration of the node.
	 * @retval 0 	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
  int (*parse)(NodeCompat *n, json_t *json);

  /* Check the current node configuration for plausability and errors.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0 	Success. Node configuration is good.
	 * @retval <0	Error. The node configuration is bogus.
	 */
  int (*check)(NodeCompat *n);

  int (*prepare)(NodeCompat *n);

  /* Returns a string with a textual represenation of this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @return	A pointer to a dynamically allocated string. Must be freed().
	 */
  char *(*print)(NodeCompat *n);

  /* Start this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
  int (*start)(NodeCompat *n);

  /* Restart this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
  int (*restart)(NodeCompat *n);

  /* Stop this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
  int (*stop)(NodeCompat *n);

  /* Pause this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
  int (*pause)(NodeCompat *n);

  /* Resume this node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
  int (*resume)(NodeCompat *n);

  /* Receive multiple messages at once.
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
  int (*read)(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

  /* Send multiple messages in a single datagram / packet.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * Messages are sent with a single sendmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages have to be stored in a circular buffer / array m.
	 * So the indexes will wrap around after len.
	 *
	 * @param n		A pointer to the node object.
	 * @param smps		An array of pointers to memory blocks where samples read from.
	 * @param cnt		The number of samples that are allocated by the calling function.
	 * @param release	The number of samples that should be released after write is called
	 * @return		The number of messages actually sent.
	 */
  int (*write)(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

  /* Reverse source and destination of a node.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @param n	A pointer to the node object.
	 */
  int (*reverse)(NodeCompat *n);

  /* Get list of file descriptors which can be used by poll/select to detect the availability of new data.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @return The number of file descriptors which have been put into \p fds.
	 */
  int (*poll_fds)(NodeCompat *n, int fds[]);

  /* Get list of socket file descriptors for configuring network emulation.
	 *
	 * This callback is optional. It will only be called if non-null.
	 *
	 * @return The number of file descriptors which have been put into \p sds.
	 */
  int (*netem_fds)(NodeCompat *n, int sds[]);

  // Return a memory allocator which should be used for sample pools passed to this node.
  struct memory::Type *(*memory_type)(NodeCompat *n,
                                      struct memory::Type *parent);
};

} // namespace node
} // namespace villas
