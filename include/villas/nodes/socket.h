/** Node type: socket
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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
#include <sys/un.h>
#include <netinet/in.h>

#ifdef __linux__
  #include <linux/if_packet.h>
#endif

#include <villas/config.h>
#include <villas/io.h>

#ifdef LIBNL3_ROUTE_FOUND
  #include <villas/kernel/if.h>
  #include <villas/kernel/nl.h>
  #include <villas/kernel/tc.h>

  #define WITH_NETEM
#endif /* LIBNL3_ROUTE_FOUND */

#include <villas/node.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct format_type;

/** The maximum length of a packet which contains stuct msg. */
#define SOCKET_INITIAL_BUFFER_LEN 1500

enum socket_layer {
	SOCKET_LAYER_ETH,
	SOCKET_LAYER_IP,
	SOCKET_LAYER_UDP,
	SOCKET_LAYER_UNIX
};

union sockaddr_union {
	struct sockaddr sa;
	struct sockaddr_storage ss;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_un sun;
#ifdef __linux__
	struct sockaddr_ll sll;
#endif
};

struct socket {
	int sd;				/**< The socket descriptor */
	int mark;			/**< Socket mark for netem, routing and filtering */
	int verify_source;		/**< Verify the source address of incoming packets against socket::remote. */

	enum socket_layer layer;	/**< The OSI / IP layer which should be used for this socket */

	union sockaddr_union local;	/**< Local address of the socket */
	union sockaddr_union remote;	/**< Remote address of the socket */

	struct format_type *format;
	struct io io;

	/* Multicast options */
	struct multicast {
		int enabled;		/**< Is multicast enabled? */
		unsigned char loop;	/** Loopback multicast packets to local host? */
		unsigned char ttl;	/**< The time to live for multicast packets. */
		struct ip_mreq mreq;	/**< A multicast group to join. */
	} multicast;

#ifdef WITH_NETEM
	struct rtnl_qdisc *tc_qdisc;	/**< libnl3: Network emulator queuing discipline */
	struct rtnl_cls *tc_classifier;	/**< libnl3: Firewall mark classifier */
#endif /* WITH_NETEM */
};


/** @see node_vtable::type_start */
int socket_type_start(struct super_node *sn);

/** @see node_type::type_stop */
int socket_type_stop();

/** @see node_type::open */
int socket_start(struct node *n);

/** @see node_type::close */
int socket_stop(struct node *n);

/** @see node_type::write */
int socket_write(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::read */
int socket_read(struct node *n, struct sample *smps[], unsigned cnt, unsigned *release);

/** @see node_type::parse */
int socket_parse(struct node *n, json_t *cfg);

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
int socket_parse_address(const char *str, struct sockaddr *sa, enum socket_layer layer, int flags);

int socket_compare_addr(struct sockaddr *x, struct sockaddr *y);

/** @} */

#ifdef __cplusplus
}
#endif
