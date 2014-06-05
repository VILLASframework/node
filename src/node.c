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

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "utils.h"
#include "msg.h"
#include "node.h"

struct node* node_create(const char *name, enum node_type type, const char *addr, int port)
{
	struct node *n = malloc(sizeof(struct node));
	if (!n)
		return NULL;

	memset(n, 0, sizeof(struct node));

	n->name = strdup(name);
	n->type = type;

	/* get ip */
	struct addrinfo *result;
	struct addrinfo hint = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = 0
	};

	int ret = getaddrinfo(addr, NULL, &hint, &result);
	if (ret) {
		print(ERROR, "Failed to get address for node %s: %s", name, gai_strerror(ret));
		return NULL;
	}

	memcpy(&n->addr, result->ai_addr, sizeof(struct sockaddr_in));
	n->addr.sin_family = AF_INET;
	n->addr.sin_port = htons(port);

	freeaddrinfo(result);

	print(DEBUG, "Node %s is reachable at %s:%u", name, inet_ntoa(n->addr.sin_addr), ntohs(n->addr.sin_port));

	/* create and connect socket */
	n->sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (n->sd < 0) {
		print(ERROR, "failed to create socket: %s", strerror(errno));
		node_destroy(n);
		return NULL;
	}

	/*ret = connect(n->sd, &n->addr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		print(ERROR, "Failed to connect socket: %s", strerror(errno));
		node_destroy(n);
		return NULL;
	}*/

	ret = bind(n->sd, &n->addr, sizeof(struct sockaddr_in));
	if (ret < 0) {
		print(ERROR, "Failed to bind socket: %s", strerror(errno));
		node_destroy(n);
		return NULL;
	}

	return n;
}

void node_destroy(struct node* n)
{
	if (!n)
		return;

	close(n->sd);

	if (n->name)
		free(n->name);

	free(n);
}

int node_send(struct node *n, struct msg *m)
{
	send(n->sd, m, sizeof(struct msg), 0);
	print(DEBUG, "Message sent to node %s", n->name);
	msg_fprint(stdout, m);
}

int node_recv(struct node *n, struct msg *m)
{
	size_t ret = recv(n->sd, m, sizeof(struct msg), 0);
	if (ret < 0)
		print(ERROR, "Recv failed: %s", strerror(errno));

	print(DEBUG, "Message received from node %s", n->name);
	msg_fprint(stdout, m);
}
