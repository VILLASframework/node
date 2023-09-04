/* Node type: socket.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>

#include <villas/node/config.hpp>

#if defined(LIBNL3_ROUTE_FOUND) && defined(__linux__)
  #define WITH_SOCKET_LAYER_ETH

  #include <linux/if_packet.h>
  #include <netinet/ether.h>
#endif // LIBNL3_ROUTE_FOUND

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

namespace villas {
namespace node {

enum class SocketLayer {
	ETH,
	IP,
	UDP,
	UNIX
};

/* Generate printable socket address depending on the address family
 *
 * A IPv4 address is formatted as dotted decimals followed by the port/protocol number
 * A link layer address is formatted in hexadecimals digits seperated by colons and the inferface name
 *
 * @param sa	A pointer to the socket address.
 * @return	The buffer containing the textual representation of the address. The caller is responsible to free() this buffer!
 */
char * socket_print_addr(struct sockaddr *saddr);

/* Parse a socket address depending on the address family
 *
 * A IPv4 address has the follwing format: [hostname/ip]:[port/protocol]
 * A link layer address has the following format: [mac]%[interface]:[ethertype]
 *
 * TODO: Add support for autodetection of address type
 *
 * @param str	A string specifiying the socket address. See description for allowed formats.
 * @param sa	A pointer to the resolved address
 * @param layer Specifies the address type in which the addr is given
 * @param flags	Flags for getaddrinfo(2)
 * @retval 0	Success. Everything went well.
 * @retval <0	Error. Something went wrong.
 */
int socket_parse_address(const char *str, struct sockaddr *sa, enum SocketLayer layer, int flags);

int socket_compare_addr(struct sockaddr *x, struct sockaddr *y);

} // namespace node
} // namespace villas
