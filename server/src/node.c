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
 */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

	/* Set socket options */
	int prio = SOCKET_PRIO;
	if (setsockopt(n->sd, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)))
		perror("Failed to set socket priority");
	else
		debug(4, "Set socket priority of node '%s' to %u", n->name, prio);

	/* Set mark for outgoing packets */
	if (setsockopt(n->sd, SOL_SOCKET, SO_MARK, &n->mark, sizeof(n->mark)))
		perror("Failed to set mark for outgoing packets");
	else
		debug(4, "Set mark of outgoing packets of node '%s' to %u", n->name, n->mark);

	/* Bind socket for receiving */
	if (bind(n->sd, (struct sockaddr *) &n->local, sizeof(struct sockaddr_in)))
		perror("Failed to bind to socket");

	return 0;
}

int node_disconnect(struct node *n)
{
	close(n->sd);

	return 0;
}

enum node_type node_lookup_type(const char *str)
{
	     if (!strcmp(str, "workstation"))
		return NODE_WORKSTATION;
	else if (!strcmp(str, "server"))
		return NODE_SERVER;
	else if (!strcmp(str, "rtds"))
		return NODE_SIM_RTDS;
	else if (!strcmp(str, "opal"))
		return NODE_SIM_OPAL;
	else if (!strcmp(str, "dsp"))
		return NODE_SIM_DSP;
	else
		return NODE_UNKNOWN;
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
