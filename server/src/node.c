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
#ifdef ENABLE_GTFPGA
  #include "gtfpga.h"
#endif
#ifdef ENABLE_OPAL_ASYNC
  #include "opal.h"
#endif
#ifdef ENABLE_SOCKET
  #include "socket.h"
  #include <netlink/route/qdisc.h>
  #include <netlink/route/classifier.h>
#endif
#ifdef ENABLE_NGSI
  #include "ngsi.h"
  #include <jansson.h>
#endif

/** Vtable for virtual node sub types */
struct list node_types = LIST_INIT(NULL);

int node_init(int argc, char *argv[], struct settings *set)
{ INDENT
	list_foreach(const struct node_type *vt, &node_types) {
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
	list_foreach(const struct node_type *vt, &node_types) {
		if (vt->refcnt) {
			info("De-initializing '%s' node type", vt->name);
			vt->deinit();
		}
	}

	return 0;
}

int node_start(struct node *n)
{ INDENT
	if (!n->refcnt) {
		warn("Node '%s' is unused. Skipping...", n->name);
		return -1;
	}

	debug(1, "Starting node '%s' of type '%s' (%s)", n->name, n->_vt->name, node_print(n));

	{ INDENT
		return node_open(n);
	}
}

int node_stop(struct node *n)
{ INDENT
	int ret;
	
	if (!n->refcnt) /* Unused and not started. No reason to stop.. */
		return -1;
	
	info("Stopping node '%s'", n->name);

	{ INDENT
		ret = node_close(n);
	}

	return ret;
}

char * node_print(struct node *n)
{
	if (!n->_print)
		n->_print = n->_vt->print(n);

	return n->_print;
}

void node_reverse(struct node *n)
{
	switch (node_type(n)) {
#ifdef ENABLE_SOCKET
		case BSD_SOCKET:
			SWAP(n->socket->remote, n->socket->local);
			break;
#endif
		case LOG_FILE:
			SWAP(n->file->read, n->file->write);
			break;
		default: { }
	}
}

struct node * node_create(struct node_type *vt)
{
	struct node *n = alloc(sizeof(struct node));
	
	n->_vt = vt;
	
	return n;
}

void node_destroy(struct node *n)
{
	switch (node_type(n)) {
#ifdef ENABLE_NGSI
		case NGSI:
			list_destroy(&n->ngsi->mapping);
		break;
#endif
#ifdef ENABLE_SOCKET
		case BSD_SOCKET:
			rtnl_qdisc_put(n->socket->tc_qdisc);
			rtnl_cls_put(n->socket->tc_classifier);
			break;
#endif
		default: { }
	}

	free(n->_print);
	free(n->socket);
	free(n);
}
