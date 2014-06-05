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
 * @file node.h
 */

#ifndef _NODE_H_
#define _NODE_H_

#include <sys/socket.h>
#include <netinet/in.h>
#include <libconfig.h>

/** The type of a node.
 *
 * This type is used to determine the message format of the remote node
 */
enum node_type
{
	NODE_INVALID,
	NODE_SERVER,
	NODE_WORKSTATION,
	NODE_SIM_OPAL,
	NODE_SIM_RTDS,
	NODE_SIM_DSP
};

/** The datastructure for a node.
 *
 * Every entity which exchanges messages is represented by a node.
 */
struct node
{
	/** The socket descriptor */
	int sd;

	/** The type of this node */
	enum node_type type;

	/** Local address of the socket */
	struct sockaddr_in local;
	/** Remote address of the socket */
	struct sockaddr_in remote;

	/** Name of the local interface */
	const char *ifname;
	/** Index of the local interface */
	int ifindex;
	/// Socket Mark
	int mark;

	/** A short identifier of the node */
	const char *name;

	/** A pointer to the libconfig object which instantiated this node */
	config_setting_t *cfg;
};

/** Create a new node.
 *
 * Memory is allocated dynamically and has to be freed by node_destroy()
 *
 * @param n A pointer to the node structure.
 * @param name An acroynm which describes the node.
 * @param type The type of a node (server, simulator, workstation).
 * @param local The local address of this node.
 *	This is where to server is listening for the arrival of new messages.
 * @param remote The local address of this node.
 * 	This is where messages are sent to.
 * @return
 *  - 0 on success
 *  - otherwise on error occured
 */
int node_create(struct node *n, const char *name, enum node_type type,
	struct sockaddr_in local, struct sockaddr_in remote);

/** Connect and bind the UDP socket of this node
 *
 * @param n A pointer to the node structure
 * @return
 *  - 0 on success
 *  - otherwise on error occured
 */
int node_connect(struct node *n);

/** Disconnect the UDP socket of this node.
 *
 * @param n A pointer to the node structure
 * @return
 *  - 0 on success
 *  - otherwise on error occured
 */
int node_disconnect(struct node *n);

/** Lookup node type from string.
 *
 * @param str The string containing the node type
 * @return The node type enumeration value
 */
enum node_type node_lookup_type(const char *str);

/** Search list of nodes for a name.
 *
 * @param str The name of the wanted node
 * @param nodes A pointer to the first list element
 * @param len Length of the node list
 * @return A pointer to the node or NULL if not found
 */
struct node* node_lookup_name(const char *str, struct node *nodes, int len);

#endif /* _NODE_H_ */
