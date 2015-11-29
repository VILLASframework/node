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

/** Vtable for virtual node sub types */
struct list node_types = LIST_INIT(NULL);

int node_parse(struct node *n, config_setting_t *cfg)
{
	return n->_vt->parse ? n->_vt->parse(n, cfg) : -1;	
}

int node_read(struct node *n, struct msg *p, int ps, int f, int c)
{
	return n->_vt->read ? n->_vt->read(n, p, ps, f, c) : -1;
}

int node_write(struct node *n, struct msg *p, int ps, int f, int c)
{
	return n->_vt->write ? n->_vt->write(n, p, ps, f, c) : -1;
}

int node_open(struct node *n)
{
	return n->_vt->open ? n->_vt->open(n) : -1;
}	
int node_close(struct node *n)
{
	return n->_vt->close ? n->_vt->close(n) : -1;
}	

int node_init(int argc, char *argv[], struct settings *set)
{ INDENT
	list_foreach(const struct node_type *vt, &node_types) {
		if (list_length(&vt->instances) > 0) {
			info("Initializing '%s' node type", vt->name);
			
			if (vt->init)
				vt->init(argc, argv, set);
		}
		else
			warn("No node is using the '%s' type. Skipping...", vt->name);
	}

	return 0;
}

int node_deinit()
{ INDENT
	/* De-initialize node types */
	list_foreach(const struct node_type *vt, &node_types) {
		if (list_length(&vt->instances) > 0) {
			info("De-initializing '%s' node type", vt->name);
			vt->deinit();
		}
	}

	return 0;
}

int node_start(struct node *n)
{ INDENT
	int ret;

	info("Starting node %s", node_name_long(n));
	{ INDENT
		ret = node_open(n);
	}
	
	if (ret == 0)
		n->state = NODE_RUNNING;
	
	return ret;
}

int node_stop(struct node *n)
{ INDENT
	int ret;

	if (n->state != NODE_RUNNING)
		return -1;

	info("Stopping node %s", node_name(n));

	{ INDENT
		ret = node_close(n);
	}
	
	if (ret == 0)
		n->state = NODE_STOPPED;

	return ret;
}

const char * node_name(struct node *n)
{
	if (!n->_name)
		strcatf(&n->_name, YEL("%s") GRY("(%s)"), n->name, n->_vt->name);
		
	return n->_name;
}

const char * node_name_long(struct node *n)
{
	if (!n->_name_long)
		n->_name_long = n->_vt->print(n);
		
	return n->_name_long;
}

int node_reverse(struct node *n)
{
	return n->_vt->reverse ? n->_vt->reverse(n) : -1;
}

struct node * node_create(struct node_type *vt)
{
	struct node *n = alloc(sizeof(struct node));
	
	list_push(&vt->instances, n);
	
	n->_vt = vt;
	n->_vd = alloc(n->_vt->size);
	
	if (n->_vt->create)
		n->_vt->create(n);

	n->state = NODE_CREATED;
	
	return n;
}

void node_destroy(struct node *n)
{
	if (n->_vt->destroy)
		n->_vt->destroy(n);

	free(n->_vd);
	free(n->_name);
	free(n);
}
