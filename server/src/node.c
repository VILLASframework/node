/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <string.h>

#include "node.h"
#include "cfg.h"
#include "utils.h"

/* Node types */
#include "socket.h"
#include "gtfpga.h"
#include "opal.h"

#define VTABLE(type, name, fnc) { type, name, config_parse_ ## fnc, \
				  fnc ## _print, \
				  fnc ## _open, \
				  fnc ## _close, \
				  fnc ## _read, \
				  fnc ## _write }

/** Vtable for virtual node sub types */
static const struct node_vtable vtables[] = {
	VTABLE(IEEE_802_3, "ieee802.3",	socket),
	VTABLE(IP,	   "ip",	socket),
	VTABLE(UDP,	   "udp",	socket),
	VTABLE(TCP,	   "tcp",	socket),
	VTABLE(TCPD,	   "tcpd",	socket),
	//VTABLE(OPAL,	   "opal",	opal  ),
	//VTABLE(GTFPGA,   "gtfpga",	gtfpga),
};

/** Linked list of nodes */
struct node *nodes;

struct node * node_lookup_name(const char *str, struct node *nodes)
{
	for (struct node *n = nodes; n; n = n->next) {
		if (!strcmp(str, n->name))
			return n;
	}

	return NULL;
}

struct node_vtable const * node_lookup_vtable(const char *str)
{
	for (int i = 0; i < ARRAY_LEN(vtables); i++) {
		if (!strcmp(vtables[i].name, str))
			return &vtables[i];
	}

	return NULL;
}

int node_start(struct node *n)
{
	int ret;

	char str[256];
	node_print(n, str, sizeof(str));
	
	debug(1, "Starting node '%s' of type '%s' (%s)", n->name, n->vt->name, str);

	{ INDENT
		if (!n->refcnt)
			warn("Node '%s' is not used by an active path", n->name);

		ret = n->vt->open(n);
	}
	
	return ret;
}

int node_start_defer(struct node *n)
{
	switch (node_type(n)) {
		case TCPD:
			info("Wait for incoming TCP connection from node '%s'...", n->name);
			listen(n->socket->sd, 1);
			n->socket->sd = accept(n->socket->sd, NULL, NULL);
			break;
	
		case TCP:
			info("Connect with TCP to remote node '%s'", n->name);
			connect(n->socket->sd, (struct sockaddr *) &n->socket->remote, sizeof(n->socket->remote));
			break;

		default:
			break;
	}
	
	return 0;
}

int node_stop(struct node *n)
{
	int ret;
	info("Stopping node '%s'", n->name);
	
	{ INDENT
		ret = n->vt->close(n);
	}
	
	return ret;
}
