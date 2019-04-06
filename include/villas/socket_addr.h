/** Node type: socket
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <villas/node/config.h>

#if defined(LIBNL3_ROUTE_FOUND) && defined(__linux__)
  #define WITH_SOCKET_LAYER_ETH

  #include <linux/if_packet.h>
  #include <netinet/ether.h>
#endif /* LIBNL3_ROUTE_FOUND */

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
#ifdef WITH_SOCKET_LAYER_ETH
	struct sockaddr_ll sll;
#endif
};

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif
