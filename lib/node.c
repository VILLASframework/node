/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <string.h>

#include "node.h"
#include "cfg.h"
#include "utils.h"

struct list node_types = LIST_INIT(NULL);			/**< Vtable for virtual node sub types */

int node_parse(struct node *n, config_setting_t *cfg)
{
	return n->_vt->parse ? n->_vt->parse(n, cfg) : 0;	
}

int node_read(struct node *n, struct pool *p, int c)
{
	return n->_vt->read ? n->_vt->read(n, p, c) : -1;
}

int node_write(struct node *n, struct pool *p, int c)
{
	return n->_vt->write ? n->_vt->write(n, p, c) : -1;
}

int node_read_single(struct node *n, struct msg *m)
{
	struct pool p = {
		.buffer = m,
		.previous = -1,
		.last = 0,
		.length = 1,
		.stride = MSG_LEN(m->values)
	};
	
	return node_read(n, &p, 1);
}

int node_write_single(struct node *n, struct msg *m)
{
	struct pool p = {
		.buffer = m,
		.previous = -1,
		.last = 0,
		.length = 1,
		.stride = MSG_LEN(m->values)
	};
	
	return node_write(n, &p, 1);
}

int node_init(struct node_type *vt, int argc, char *argv[], config_setting_t *cfg)
{
	if (list_length(&vt->instances) > 0) {
		info("Initializing " YEL("%s") " node type", vt->name);
			
		if (vt->init) { INDENT
			vt->init(argc, argv, cfg);
		}
	}
	else
		warn("No node is using the " YEL("%s") " node type. Skipping...", vt->name);

	return 0;
}

int node_deinit(struct node_type *vt)
{
	if (list_length(&vt->instances) > 0) {
		info("De-initializing " YEL("%s") " node type", vt->name);

		if (vt->deinit) { INDENT
			vt->deinit();
		}
	}

	return 0;
}

int node_start(struct node *n)
{
	int ret;

	info("Starting node %s", node_name_long(n));
	{ INDENT
		ret = n->_vt->open ? n->_vt->open(n) : -1;
	}
	
	if (ret == 0)
		n->state = NODE_RUNNING;
	
	return ret;
}

int node_stop(struct node *n)
{
	int ret;

	if (n->state != NODE_RUNNING)
		return -1;

	info("Stopping node %s", node_name(n));

	{ INDENT
		ret = n->_vt->close ? n->_vt->close(n) : -1;
	}
	
	if (ret == 0)
		n->state = NODE_STOPPED;

	return ret;
}

char * node_name(struct node *n)
{
	if (!n->_name)
		strcatf(&n->_name, RED("%s") "(" YEL("%s") ")", n->name, n->_vt->name);
		
	return n->_name;
}

char * node_name_long(struct node *n)
{
	if (!n->_name_long) {
		if (n->_vt->print) {
			char *name_long = n->_vt->print(n);
			strcatf(&n->_name_long, "%s: %s", node_name(n), name_long);
			free(name_long);
		}
		else
			n->_name_long = node_name(n);		
	}
		
	return n->_name_long;
}

const char * node_name_short(struct node *n)
{
	return n->name;
}

const char * node_name_type(struct node *n)
{
	return n->_vt->name;
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
	
	list_remove(&n->_vt->instances, n);

	free(n->_vd);
	free(n->_name);
	free(n);
}
