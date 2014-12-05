/** Nodes
 *
 * The S2SS server connects multiple nodes.
 * There are multiple types of nodes:
 *  - simulators
 *  - servers
 *  - workstations
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _NODE_H_
#define _NODE_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <libconfig.h>

#include "msg.h"
#include "tc.h"

/** Static node initialization */
#define NODE_INIT(n)	{ \
	.name = n \
}

/* Helper macros for virtual node type */
#define node_type(n)		((n)->vt->type)
#define node_print(n, b, l)	((n)->vt->print(n, b, l))
#define node_read(n, m)		((n)->vt->read(n, m))
#define node_write(n, m)	((n)->vt->write(n, m))

/** Node type: layer, protocol, listen/connect */
enum node_type {
	IEEE_802_3,	/* BSD socket: AF_PACKET SOCK_DGRAM  */
	IP,		/* BSD socket: AF_INET   SOCK_RAW    */
	UDP,		/* BSD socket: AF_INET   SOCK_DGRAM  */
	TCPD,		/* BSD socket: AF_INET   SOCK_STREAM bind + listen + accept */
	TCP,		/* BSD socket: AF_INET   SOCK_STREAM bind + connect */
//	OPAL_ASYNC,	/* OPAL-RT AsyncApi */
//	GTFPGA,		/* Xilinx ML507 GTFPGA card */
	INVALID
};

/** C++ like vtable construct for socket_types */
struct node_vtable {
	enum node_type type;
	const char *name;

	int (*parse)(config_setting_t *cfg, struct node *n);
	int (*print)(struct node *n, char *buf, int len);

	int (*open)(struct node *n);
	int (*close)(struct node *n);
	int (*read)(struct node *n, struct msg *m);
	int (*write)(struct node *n, struct msg *m);
};

/** The data structure for a node.
 *
 * Every entity which exchanges messages is represented by a node.
 * Nodes can be remote machines and simulators or locally running processes.
 */
struct node
{
	/** How many paths  are sending / receiving from this node? */
	int refcnt;
	/** A short identifier of the node, only used for configuration and logging */
	const char *name;

	/** C++ like virtual function call table */
	struct node_vtable const *vt;
	/** Virtual data (used by vtable functions) */
	union {
		struct socket *socket;
		struct opal *opal;
		struct gtfpga *gtfpga;
	};

	/** A pointer to the libconfig object which instantiated this node */
	config_setting_t *cfg;

	/** Linked list pointer */
	struct node *next;
};

/** Connect and bind the UDP socket of this node.
 *
 * Depending on the type (vtable) of this node,
 *  a socket is opened, shmem region registred...
 *
 * @param n A pointer to the node structure
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int node_start(struct node *n);

/** Deferred TCP connection setup
 *
 * @todo Dirty hack!
 *  We should check the address of the connecting node!
 *  We should preserve the original socket for proper shutdown.
 */
int node_start_defer(struct node *n);

/** Stops a node.
 *
 * Depending on the type (vtable) of this node,
 *  a socket is closed, shmem region released...
 *
 * @param n A pointer to the node structure
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int node_stop(struct node *n);

/** Lookup string representation of socket type
 *
 * @param type A string describing the socket type. This must be one of: tcp, tcpd, udp, ip, ieee802.3
 * @return An enumeration value or INVALID (0)
 */
struct node_vtable const * node_lookup_vtable(const char *str);

/** Search list of nodes for a name.
 *
 * @param str The name of the wanted node
 * @param nodes A linked list of all nodes
 * @return A pointer to the node or NULL if not found
 */
struct node* node_lookup_name(const char *str, struct node *nodes);

#endif /* _NODE_H_ */
