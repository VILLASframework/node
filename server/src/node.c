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
#ifdef ENABLE_OPAL_ASYNC
#include "opal.h"
#endif

#define VTABLE(type, name, fnc) { type, name, config_parse_ ## fnc, \
				  fnc ## _print, \
				  fnc ## _open, \
				  fnc ## _close, \
				  fnc ## _read, \
				  fnc ## _write }

/** Vtable for virtual node sub types */
static const struct node_vtable vtables[] = {
#ifdef ENABLE_OPAL_ASYNC
	VTABLE(OPAL_ASYNC, "opal",	opal),
#endif
	VTABLE(IEEE_802_3, "ieee802.3",	socket),
	VTABLE(IP,	   "ip",	socket),
	VTABLE(UDP,	   "udp",	socket),
	VTABLE(TCP,	   "tcp",	socket),
	VTABLE(TCPD,	   "tcpd",	socket)
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
	if (!n->refcnt) {
		warn("Node '%s' is unused. Skipping...", n->name);
		return -1;
	}

	char str[256];
	node_print(n, str, sizeof(str));
	
	debug(1, "Starting node '%s' of type '%s' (%s)", n->name, n->vt->name, str);

	{ INDENT
		return n->vt->open(n);
	}
}

int node_start_defer(struct node *n)
{
	int ret;

	if (node_type(n) == TCPD) {
		info("Wait for incoming TCP connection from node '%s'...", n->name);

		ret = listen(n->socket->sd2, 1);
		if (ret < 0)
			serror("Failed to listen on socket for node '%s'", n->name);
			
		ret = accept(n->socket->sd2, NULL, NULL);
		if (ret < 0)
			serror("Failed to accept on socket for node '%s'", n->name);
			
		n->socket->sd = ret;	
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

int node_reverse(struct node *n)
{
	switch (n->vt->type) {
		case IEEE_802_3:
		case IP:
		case UDP:
		case TCP:
			SWAP(n->socket->remote, n->socket->local);
			break;
		default: { }
	}
	return n->vt->open == socket_open;
}
