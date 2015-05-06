/** Node type: socket
 *
 * This file implements the socket subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 * @addtogroup socket BSD Socket Node Type
 * @{
 */

#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <sys/socket.h>

#include "node.h"

struct socket {
	/** The socket descriptor */
	int sd;
	/** The socket descriptor for an established TCP connection */
	int sd2;
	/** Socket mark for netem, routing and filtering */
	int mark;

	/** Local address of the socket */
	struct sockaddr_storage local;
	/** Remote address of the socket */
	struct sockaddr_storage remote;

	/** Network emulator settings */
	struct netem *netem;

	/* Linked list _per_interface_ */
	struct socket *next;
};


/** @see node_vtable::init */
int socket_init(int argc, char *argv[], struct settings *set);

/** @see node_vtable::deinit */
int socket_deinit();

/** @see node_vtable::open */
int socket_open(struct node *n);

/** @see node_vtable::close */
int socket_close(struct node *n);	

/** @see node_vtable::write */
int socket_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

/** @see node_vtable::read */
int socket_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt);

/** @see node_vtable::parse */
int socket_parse(config_setting_t *cfg, struct node *n);

/** @see node_vtable::print */
int socket_print(struct node *n, char *buf, int len);

/** Generate printable socket address depending on the address family
 *
 * A IPv4 address is formatted as dotted decimals followed by the port/protocol number
 * A link layer address is formatted in hexadecimals digits seperated by colons and the inferface name
 *
 * @param buf	A buffer to be filled.
 * @param len	The length of the supplied buffer.
 * @param sa	A pointer to the socket address.
 * @retur	The length of the address in bytes.
 */
int socket_print_addr(char *buf, int len, struct sockaddr *sa);

/** Parse a socket address depending on the address family
 *
 * A IPv4 address has the follwing format: [hostname/ip]:[port/protocol]
 * A link layer address has the following format: [mac]%[interface]:[ethertype]
 *
 * @todo Add support for autodetection of address type
 *
 * @param str	A string specifiying the socket address. See description for allowed formats.
 * @param sa	A pointer to the resolved address
 * @param type	Specifies the address type in which the addr is given
 * @param flags	Flags for getaddrinfo(2)
 * @retval 0	Success. Everything went well.
 * @retval <0	Error. Something went wrong.
 */
int socket_parse_addr(const char *str, struct sockaddr *sa, enum node_type type, int flags);

#endif /** _SOCKET_H_ @} */
