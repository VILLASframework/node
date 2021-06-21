/** The socket node-type for Layer 2, 3, 4 BSD-style sockets
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
 * @addtogroup socket BSD Socket Node Type
 * @ingroup node
 * @{
 */

#pragma once

#include <villas/node/config.h>
#include <villas/socket_addr.h>
#include <villas/format.hpp>

/* Forward declarations */
struct vnode;

/** The maximum length of a packet which contains stuct msg. */
#define SOCKET_INITIAL_BUFFER_LEN (64*1024)

struct socket {
	int sd;				/**< The socket descriptor */
	int verify_source;		/**< Verify the source address of incoming packets against socket::remote. */

	enum SocketLayer layer;		/**< The OSI / IP layer which should be used for this socket */

	villas::node::Format *formatter;

	/* Multicast options */
	struct multicast {
		int enabled;		/**< Is multicast enabled? */
		unsigned char loop;	/** Loopback multicast packets to local host? */
		unsigned char ttl;	/**< The time to live for multicast packets. */
		struct ip_mreq mreq;	/**< A multicast group to join. */
	} multicast;

	struct {
		char *buf;		/**< Buffer for receiving messages */
		size_t buflen;
		union sockaddr_union saddr;	/**< Remote address of the socket */
	} in, out;
};


/** @see node_vtable::type_start */
int socket_type_start(villas::node::SuperNode *sn);

/** @see node_type::type_stop */
int socket_type_stop();

/** @see node_type::start */
int socket_start(struct vnode *n);

/** @see node_type::stop */
int socket_stop(struct vnode *n);

/** @see node_type::write */
int socket_write(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::read */
int socket_read(struct vnode *n, struct sample * const smps[], unsigned cnt);

/** @see node_type::parse */
int socket_parse(struct vnode *n, json_t *json);

/** @see node_type::print */
char * socket_print(struct vnode *n);

/** @} */
