/** Node type: socket
 *
 * This file implements the socket subtype for nodes.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 */
/**
 * @addtogroup socket BSD Socket Node Type
 * @ingroup node
 * @{
 *********************************************************************************/

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <sys/socket.h>
#include <linux/if_packet.h>

#include "node.h"

enum socket_layer {
	LAYER_ETH,
	LAYER_IP,
	LAYER_UDP
};

enum app_hdr_type {
	APP_HDR_NONE,			/** No header in the payload */
	APP_HDR_GTSKT,			/** No header in the payload, same as HDR_NONE*/
	APP_HDR_DEFAULT			/** Default header in the payload, (see msg_format.h) */
};

union sockaddr_union {
	struct sockaddr sa;
	struct sockaddr_in sin;
	struct sockaddr_in6 sin6;
	struct sockaddr_ll sll;
};

struct socket {
	/** The socket descriptor */
	int sd;
	/** Socket mark for netem, routing and filtering */
	int mark;

	/** The OSI / IP layer which should be used for this socket */
	enum socket_layer layer;

	/** Local address of the socket */
	union sockaddr_union local;
	/** Remote address of the socket */
	union sockaddr_union remote;
	/** Payload header type */
	enum app_hdr_type app_hdr;

	/** libnl3: Network emulator queuing discipline */
	struct rtnl_qdisc *tc_qdisc;
	/** libnl3: Firewall mark classifier */
	struct rtnl_cls *tc_classifier;

	/* Linked list _per_interface_ */
	struct socket *next;
};


/** @see node_vtable::init */
int socket_init(int argc, char *argv[], config_setting_t *cfg);

/** @see node_vtable::deinit */
int socket_deinit();

/** @see node_vtable::open */
int socket_open(struct node *n);

/** @see node_vtable::close */
int socket_close(struct node *n);

/** @see node_vtable::write */
int socket_write(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_vtable::read */
int socket_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_vtable::parse */
int socket_parse(struct node *n, config_setting_t *cfg);

/** @see node_vtable::print */
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

#endif /** _SOCKET_H_ @} */
