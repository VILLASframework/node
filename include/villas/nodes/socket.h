/** Node type: socket
 *
 * This file implements the socket subtype for nodes.
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
 *********************************************************************************/

/**
 * @addtogroup socket BSD Socket Node Type
 * @ingroup node
 * @{
 */

#pragma once

#include <sys/socket.h>
#include <linux/if_packet.h>

#include "node.h"

enum socket_layer {
	SOCKET_LAYER_ETH,
	SOCKET_LAYER_IP,
	SOCKET_LAYER_UDP
};

enum socket_header {
	SOCKET_HEADER_DEFAULT,	/**> Default header in the payload, (see msg_format.h) */
	SOCKET_HEADER_NONE,	/**> No header in the payload, same as HDR_NONE*/
	SOCKET_HEADER_FAKE	/**> Same as SOCKET_HEADER_NONE but using the first three data values as: sequence, seconds & nano-seconds. */
};

union sockaddr_union {
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_ll sll;
};

struct socket {
	int sd;				/**> The socket descriptor */
	int mark;			/**> Socket mark for netem, routing and filtering */

	enum {
		SOCKET_ENDIAN_LITTLE,
		SOCKET_ENDIAN_BIG
	} endian;			/** Endianness of the data sent/received by the node */

	enum socket_layer layer;	/**> The OSI / IP layer which should be used for this socket */
	enum socket_header header;	/**> Payload header type */

	union sockaddr_union local;	/**> Local address of the socket */
	union sockaddr_union remote;	/**> Remote address of the socket */

	struct rtnl_qdisc *tc_qdisc;	/**> libnl3: Network emulator queuing discipline */
	struct rtnl_cls *tc_classifier;	/**> libnl3: Firewall mark classifier */

	struct socket *next;		/* Linked list _per_interface_ */
};


/** @see node_vtable::init */
int socket_init(struct super_node *sn);

/** @see node_type::deinit */
int socket_deinit();

/** @see node_type::open */
int socket_start(struct node *n);

/** @see node_type::close */
int socket_stop(struct node *n);

/** @see node_type::write */
int socket_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::read */
int socket_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_type::parse */
int socket_parse(struct node *n, config_setting_t *cfg);

/** @see node_type::print */
char * socket_print(struct node *n);

/** Generate printable socket address depending on the address family
 *
 * A IPv4 address is formatted as dotted decimals followed by the port/protocol number
 * A link layer address is formatted in hexadecimals digits seperated by colons and the inferface name
 *
 * @param sa	A pointer to the socket address.
 * @return	The buffer containing the textual representation of the address. The caller is responsible to free() this buffer!
 */
char * socket_print_addr(struct sockaddr *saddr);

/** Parse a socket address depending on the address family
 *
 * A IPv4 address has the follwing format: [hostname/ip]:[port/protocol]
 * A link layer address has the following format: [mac]%[interface]:[ethertype]
 *
 * @todo Add support for autodetection of address type
 *
 * @param str	A string specifiying the socket address. See description for allowed formats.
 * @param sa	A pointer to the resolved address
 * @param layer Specifies the address type in which the addr is given
 * @param flags	Flags for getaddrinfo(2)
 * @retval 0	Success. Everything went well.
 * @retval <0	Error. Something went wrong.
 */
int socket_parse_addr(const char *str, struct sockaddr *sa, enum socket_layer layer, int flags);

/** @} */