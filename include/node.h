/**
 * Nodes
 *
 * The S2SS server connects multiple nodes.
 * There are multiple types of nodes:
 *  - simulators
 *  - servers
 *  - workstations
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _NODE_H_
#define _NODE_H_

#include <sys/socket.h>
#include <netinet/in.h>

enum node_type
{
	SIMULATOR,
	SERVER,
	WORKSTATION
};

struct node
{
	/// The socket descriptor
	int sd;

	// Local address of the socket
	struct sockaddr_in local;

	// Remote address of the socket
	struct sockaddr_in remote;

	/// The type of this node
	enum node_type type;

	/// A short identifier of the node
	char *name;
};

struct msg; /* forward decl */

/**
 * @brief Create a new node
 *
 * Memory is allocated dynamically and has to be freed by node_destroy()
 *
 * @param name An acroynm, describing the node
 * @param type The type of a node (SERVER, SIMULATOR, WORKSTATION)
 * @param local A string specifying the local ip:port
 * @param remote A string specifying the remote ip:port
 * @return
 *  - 0 on success
 *  - otherwise on error occured
 */
struct node* node_create(const char *name, enum node_type type, const char *local, const char *remote);

/**
 * @brief Delete a node created by node_create()
 *
 * @param p A pointer to the node struct
 */
void node_destroy(struct node* n);

/**
 *
 */

/**
 *
 */

#endif /* _NODE_H_ */
