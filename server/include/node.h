/** Nodes
 *
 * The S2SS server connects multiple nodes.
 * There are multiple types of nodes:
 *  - simulators
 *  - servers
 *  - workstations
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _NODE_H_
#define _NODE_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <libconfig.h>

#include "msg.h"
#include "tc.h"
#include "list.h"

/** Static node initialization */
#define NODE_INIT(n)	{ \
	.name = n \
}

/* Helper macros for virtual node type */
#define node_type(n)			((n)->vt->type)
#define node_print(n, b, l)		((n)->vt->print(n, b, l))

#define node_read(n, p, ps, f, c)	((n)->vt->read(n, p, ps, f, c))
#define node_write(n, p, ps, f, c)	((n)->vt->write(n, p, ps, f, c))

#define node_read_single(n, m)		((n)->vt->read(n, m, 1, 0, 1))
#define node_write_single(n, m)		((n)->vt->write(n, m, 1, 0, 1))

/** Node type: layer, protocol, listen/connect */
enum node_type {
	LOG_FILE,		/**< File IO */
	OPAL_ASYNC,		/**< OPAL-RT Asynchronous Process Api */
	GTFPGA,			/**< Xilinx ML507 GTFPGA card */
	IEEE_802_3 = 0x10,	/**< BSD socket: AF_PACKET SOCK_DGRAM  */
	IP,			/**< BSD socket: AF_INET   SOCK_RAW    */
	UDP,			/**< BSD socket: AF_INET   SOCK_DGRAM  */
	TCPD,			/**< BSD socket: AF_INET   SOCK_STREAM bind + listen + accept */
	TCP,			/**< BSD socket: AF_INET   SOCK_STREAM bind + connect */

	SOCKET = 0xF0		/**< Mask for BSD socket types */
};

/** C++ like vtable construct for node_types
 * @todo Add comments
 */
struct node_vtable {
	const enum node_type type;
	const char *name;

	/** Parse node connection details for SOCKET type
	 *
	 * @param cfg	A libconfig object pointing to the node.
	 * @param n	A pointer to the node structure which should be parsed.
	 * @retval 0 	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*parse)(config_setting_t *cfg, struct node *n);
	
	/** Print details of socket connection
	 *
	 * @param n	A pointer to the node structure
	 * @param buf	The character buffer to be filled.
	 * @param len	The length of the supplied buffer.
	 * @return The	length of the address.
	 */
	int (*print)(struct node *n, char *buf, int len);

	/** Create new socket and connect(), bind(), accept().
	 *
	 * @param n	A pointer to the node.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*open) (struct node *n);
	
	/** Close the socket.
	 *
	 * @param n	A pointer to the node.
	 * @retval 0	Success. Everything went well.
	 * @retval <0	Error. Something went wrong.
	 */
	int (*close)(struct node *n);
	
	/** Receive multiple messages from single datagram / packet.
	 *
	 * Messages are received with a single recvmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages will be stored in a circular buffer / array @p m.
	 * Indexes used to address @p m will wrap around after len messages.
	 *
	 * @param n 		A pointer to the node where the messages should be sent to.
	 * @param pool 		A pointer to an array of messages which should be sent.
	 * @param poolsize 	The length of the message array.
	 * @param first		The index of the first message which should be sent.
	 * @param cnt		The number of messages which should be sent.
	 * @return		The number of messages actually received.
	 */
	int (*read) (struct node *n, struct msg *pool, int poolsize, int first, int cnt);
	
	/** Send multiple messages in a single datagram / packet.
	 *
	 * Messages are sent with a single sendmsg() syscall by
	 * using gathering techniques (struct iovec).
	 * The messages have to be stored in a circular buffer / array m.
	 * So the indexes will wrap around after len.
	 *
	 * @param n		A pointer to the node where the messages should be sent to.
	 * @param pool		A pointer to an array of messages which should be sent.
	 * @param poolsize	The length of the message array.
	 * @param first		The index of the first message which should be sent.
	 * @param cnt		The number of messages which should be sent.
	 * @return		The number of messages actually sent.
	 */
	int (*write)(struct node *n, struct msg *pool, int poolsize, int first, int cnt);
	
	int (*init)(int argc, char *argv[], struct settings *set);
	int (*deinit)();
	
	int refcnt;
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
	/** Number of messages to send / recv at once (scatter / gather) */
	int combine;
	/** A short identifier of the node, only used for configuration and logging */
	const char *name;

	/** C++ like virtual function call table */
	struct node_vtable *vt;
	/** Virtual data (used by vtable functions) */
	union {
		struct socket *socket;
		struct opal   *opal;
		struct gtfpga *gtfpga;
		struct file   *file;
	};

	/** A pointer to the libconfig object which instantiated this node */
	config_setting_t *cfg;
};

/** Initialize node type subsystems.
 *
 * These routines are only called once per type (not per node instance).
 * See node_vtable::init
 */
int node_init(int argc, char *argv[], struct settings *set);

/** De-initialize node type subsystems.
 *
 * These routines are only called once per type (not per node instance).
 * See node_vtable::deinit
 */
int node_deinit();

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
 * @param str A string describing the socket type. This must be one of: tcp, tcpd, udp, ip, ieee802.3 or opal
 * @return A pointer to the vtable, or NULL if there is no socket type / vtable with this id.
 */
struct node_vtable * node_lookup_vtable(const char *str);

/** Search list of nodes for a name.
 *
 * @param str The name of the wanted node
 * @param nodes A linked list of all nodes
 * @return A pointer to the node or NULL if not found
 */
struct node * node_lookup_name(const char *str, struct list *nodes);

/** Reverse local and remote socket address.
 * This is usefull for the helper programs: send, receive, test
 * because they usually use the same configuration file as the
 * server and therefore the direction needs to be swapped. */
void node_reverse(struct node *n);

/** Create a node by allocating dynamic memory. */
struct node * node_create();

/** Destroy node by freeing dynamically allocated memory.
 *
 * @param i A pointer to the interface structure.
 */
void node_destroy(struct node *n);

#endif /* _NODE_H_ */
