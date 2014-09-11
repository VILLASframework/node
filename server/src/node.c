/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "config.h"
#include "cfg.h"
#include "utils.h"
#include "msg.h"
#include "node.h"
#include "if.h"

int node_connect(struct node *n)
{
	/* Create socket */
	n->sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (n->sd < 0)
		perror("Failed to create socket");

	/* Bind socket for receiving */
	if (bind(n->sd, (struct sockaddr *) &n->local, sizeof(struct sockaddr_in)))
		perror("Failed to bind to socket");

	debug(1, "  We listen for node '%s' at %s:%u",
		n->name, inet_ntoa(n->local.sin_addr),
		ntohs(n->local.sin_port));
	debug(1, "  We sent to node '%s' at %s:%u",
		n->name, inet_ntoa(n->remote.sin_addr),
		ntohs(n->remote.sin_port));

	return 0;
}

int node_disconnect(struct node *n)
{
	close(n->sd);

	return 0;
}

struct node* node_lookup_name(const char *str, struct node *nodes)
{
	for (struct node *n = nodes; n; n = n->next) {
		if (!strcmp(str, n->name)) {
			return n;
		}
	}

	return NULL;
}
