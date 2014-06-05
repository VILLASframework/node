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

struct node* node_create(const char *name, enum node_type type, const char *local, const char *remote)
{
	int ret;
	struct node *n = malloc(sizeof(struct node));
	if (!n)
		return NULL;

	memset(n, 0, sizeof(struct node));

	n->name = strdup(name);
	n->type = type;

	resolve(local, &n->local);
	resolve(remote, &n->remote);

	/* create and connect socket */
	n->sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (n->sd < 0) {
		node_destroy(n);
		print(FATAL, "Failed to create socket: %s", strerror(errno));
		return NULL;
	}

	ret = bind(n->sd, &n->local, sizeof(struct sockaddr_in));
	if (ret < 0) {
		node_destroy(n);
		print(FATAL, "Failed to bind socket: %s", strerror(errno));
		return NULL;
	}

	ret = connect(n->sd, &n->remote, sizeof(struct sockaddr_in));
	if (ret < 0) {
		node_destroy(n);
		print(FATAL, "Failed to connect socket: %s", strerror(errno));
		return NULL;
	}

	print(DEBUG, "We listen for node %s at %s:%u", name, inet_ntoa(n->local.sin_addr), ntohs(n->local.sin_port));
	print(DEBUG, "We sent to node %s at %s:%u", name, inet_ntoa(n->remote.sin_addr), ntohs(n->remote.sin_port));

	return n;
}

int resolve(const char *addr, struct sockaddr_in *sa)
{
	/* split host:port */
	char *host;
	char *port;

	if (sscanf(addr, "%m[^:]:%ms", &host, &port) != 2) {
		print(FATAL, "Invalid address format: %s", addr);
	}

	/* get ip */
	struct addrinfo *result;
	struct addrinfo hint = {
		.ai_family = AF_INET,
		.ai_socktype = SOCK_DGRAM,
		.ai_protocol = 0
	};

	int ret = getaddrinfo(host, port, &hint, &result);
	if (ret) {
		print(FATAL, "Failed to get address for node %s: %s", addr, gai_strerror(ret));
		return -EINVAL;
	}

	memcpy(sa, result->ai_addr, sizeof(struct sockaddr_in));
	sa->sin_family = AF_INET;
	sa->sin_port = htons(atoi(port));

	freeaddrinfo(result);
	free(host);
	free(port);

	return 0;
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
