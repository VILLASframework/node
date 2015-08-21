/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <string.h>

#include "node.h"
#include "cfg.h"
#include "utils.h"

/* Node types */
#include "file.h"
#include "socket.h"

#ifdef ENABLE_GTFPGA
  #include "gtfpga.h"
#endif
#ifdef ENABLE_OPAL_ASYNC
  #include "opal.h"
#endif

#define VTABLE(type, name, fnc) { type, name, \
				  fnc ## _parse, fnc ## _print, \
				  fnc ## _open,  fnc ## _close, \
				  fnc ## _read,  fnc ## _write, \
				  fnc ## _init,  fnc ## _deinit }

/** Vtable for virtual node sub types */
struct node_vtable vtables[] = {
#ifdef ENABLE_OPAL_ASYNC
	VTABLE(OPAL_ASYNC, "opal",	opal),
#endif
#ifdef ENABLE_GTFPGA
	VTABLE(GTFPGA,	   "gtfpga",	gtfpga),
#endif
	VTABLE(BSD_SOCKET, "socket",	socket),
	VTABLE(LOG_FILE,   "file",	file)
};

int node_init(int argc, char *argv[], struct settings *set)
{ INDENT
	for (int i=0; i<ARRAY_LEN(vtables); i++) {
		const struct node_vtable *vt = &vtables[i];
		if (vt->refcnt) {
			info("Initializing '%s' node type", vt->name);
			vt->init(argc, argv, set);
		}
	}

	return 0;
}

int node_deinit()
{ INDENT
	/* De-initialize node types */
	for (int i=0; i<ARRAY_LEN(vtables); i++) {
		struct node_vtable *vt = &vtables[i];
		if (vt->refcnt) {
			info("De-initializing '%s' node type", vt->name);
			vt->deinit();
		}
	}
	return 0;
}

struct node * node_lookup_name(const char *str, struct list *nodes)
{
	FOREACH(nodes, it) {
		if (!strcmp(str, it->node->name))
			return it->node;
	}

	return NULL;
}

struct node_vtable * node_lookup_vtable(const char *str)
{
	for (int i = 0; i < ARRAY_LEN(vtables); i++) {
		if (!strcmp(vtables[i].name, str))
			return &vtables[i];
	}

	return NULL;
}

int node_start(struct node *n)
{ INDENT
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

int node_stop(struct node *n)
{ INDENT
	int ret;
	info("Stopping node '%s'", n->name);

	{ INDENT
		ret = n->vt->close(n);
	}

	return ret;
}

void node_reverse(struct node *n)
{
	switch (n->vt->type) {
		case BSD_SOCKET:
			SWAP(n->socket->remote, n->socket->local);
			break;
		case LOG_FILE:
			SWAP(n->file->path_in, n->file->path_out);
			break;
		default: { }
	}
}

struct node * node_create()
{
	return alloc(sizeof(struct node));
}

void node_destroy(struct node *n)
{
	switch (n->vt->type) {
		case BSD_SOCKET:
			free(n->socket->netem);
			break;
		case LOG_FILE:
			free(n->file->path_in);
			free(n->file->path_out);
			break;
		default: { }
	}

	free(n->socket);
	free(n);
}
