/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <string.h>

#include "sample.h"
#include "node.h"
#include "cfg.h"
#include "utils.h"

/** List of registered node-types */
struct list node_types = LIST_INIT();

int node_parse(struct node *n, config_setting_t *cfg)
{
	return n->_vt->parse ? n->_vt->parse(n, cfg) : 0;	
}

int node_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int nread = 0;

	if (!n->_vt->read)
		return -1;

	/* Send in parts if vector not supported */
	if (n->_vt->vectorize > 0 && n->_vt->vectorize < cnt) {
		while (cnt - nread > 0) {
			nread += n->_vt->read(n, &smps[nread], MIN(cnt - nread, n->_vt->vectorize));
		}
	}
	else {
		nread = n->_vt->read(n, smps, cnt);
	}
	
	return nread;
}

int node_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int nsent = 0;

	if (!n->_vt->write)
		return -1;

	/* Send in parts if vector not supported */
	if (n->_vt->vectorize > 0 && n->_vt->vectorize < cnt) {
		while (cnt - nsent > 0)
			nsent += n->_vt->write(n, &smps[nsent], MIN(cnt - nsent, n->_vt->vectorize));
	}
	else {
		nsent = n->_vt->write(n, smps, cnt);
	}
	
	return nsent;
}

int node_init(struct node_type *vt, int argc, char *argv[], config_setting_t *cfg)
{
	int ret;
	
	if (vt->state != NODE_TYPE_UNINITIALIZED)
		return -1;

	info("Initializing " YEL("%s") " node type", vt->name);
	{ INDENT
		ret = vt->init ? vt->init(argc, argv, cfg) : -1;
	}	

	if (ret == 0)
		vt->state = NODE_TYPE_INITIALIZED;

	return ret;
}

int node_deinit(struct node_type *vt)
{
	int ret;
	
	if (vt->state != NODE_TYPE_INITIALIZED)
		return -1;

	info("De-initializing " YEL("%s") " node type", vt->name);
	{ INDENT
		ret = vt->deinit ? vt->deinit() : -1;
	}
	
	if (ret == 0)
		vt->state = NODE_TYPE_UNINITIALIZED;

	return ret;
}

int node_start(struct node *n)
{
	int ret;
	
	if (n->state != NODE_CREATED && n->state != NODE_STOPPED)
		return -1;
	
	n->state = NODE_STARTING;

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
	
	n->state = NODE_STOPPING;

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
