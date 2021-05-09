/** Node type: ZeroMQ
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
 * @addtogroup zeromq ZeroMQ node type
 * @ingroup node
 * @{
 */

#pragma once

#include <cstdint>
#include <jansson.h>

#include <villas/super_node.hpp>
#include <villas/list.h>
#include <villas/format.hpp>

#if ZMQ_BUILD_DRAFT_API && (ZMQ_VERSION_MAJOR > 4 || (ZMQ_VERSION_MAJOR == 4 && ZMQ_VERSION_MINOR >= 2))
  #define ZMQ_BUILD_DISH 1
#endif

/* Forward declarations */
struct vnode;
struct sample;

struct zeromq {
	int ipv6;

	villas::node::Format *formatter;

	struct Curve {
		int enabled;
		struct {
			char public_key[41];
			char secret_key[41];
		} server, client;
	} curve;

	enum class Pattern {
		PUBSUB,
#ifdef ZMQ_BUILD_DISH
		RADIODISH
#endif
	} pattern;

	struct Dir {
		void *socket;	/**< ZeroMQ socket. */
		void *mon_socket;
		struct vlist endpoints;
		char *filter;
		int bind, pending;
	} in, out;
};

/** @see node_type::print */
char * zeromq_print(struct vnode *n);

/** @see node_type::parse */
int zeromq_parse(struct vnode *n, json_t *json);

/** @see node_type::type_start */
int zeromq_type_start(villas::node::SuperNode *sn);

/** @see node_type::type_stop */
int zeromq_type_stop();

/** @see node_type::start */
int zeromq_start(struct vnode *n);

/** @see node_type::stop */
int zeromq_stop(struct vnode *n);

/** @see node_type::read */
int zeromq_read(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::write */
int zeromq_write(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @} */
