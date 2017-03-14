/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <string.h>
#include <libconfig.h>

#include "sample.h"
#include "node.h"
#include "utils.h"
#include "config.h"
#include "plugin.h"

int node_init(struct node *n, struct node_type *vt)
{
	assert(n->state == STATE_DESTROYED);

	n->_vt = vt;
	n->_vd = alloc(vt->size);
	
	list_push(&vt->instances, n);

	n->state = STATE_INITIALIZED;

	return 0;
}

int node_parse(struct node *n, config_setting_t *cfg)
{
	struct plugin *p;
	const char *type, *name;
	int ret;

	name = config_setting_name(cfg);
	
	if (!config_setting_lookup_string(cfg, "type", &type))
		cerror(cfg, "Missing node type");
	
	p = plugin_lookup(PLUGIN_TYPE_NODE, type);
	assert(&p->node == n->_vt);
	
	if (!config_setting_lookup_int(cfg, "vectorize", &n->vectorize))
		n->vectorize = 1;

	n->name = name;
	n->cfg = cfg;

	ret = n->_vt->parse ? n->_vt->parse(n, cfg) : 0;
	if (ret)
		cerror(cfg, "Failed to parse node '%s'", node_name(n));
	
	n->state = STATE_PARSED;

	return ret;
}

int node_check(struct node *n)
{
	assert(n->state != STATE_DESTROYED);

	if (n->vectorize <= 0)
		error("Invalid `vectorize` value %d for node %s. Must be natural number!", n->vectorize, node_name(n));

	if (n->_vt->vectorize && n->_vt->vectorize < n->vectorize)
		error("Invalid value for `vectorize`. Node type requires a number smaller than %d!",
			n->_vt->vectorize);

	n->state = STATE_CHECKED;

	return 0;
}

int node_start(struct node *n)
{
	int ret;
	
	assert(n->state == STATE_CHECKED);

	info("Starting node %s", node_name_long(n));
	{ INDENT
		ret = n->_vt->start ? n->_vt->start(n) : -1;
		if (ret)
			return ret;
	}

	n->state = STATE_STARTED;
	
	n->sequence = 0;
	
	return ret;
}

int node_stop(struct node *n)
{
	int ret;

	assert(n->state == STATE_STARTED);

	info("Stopping node %s", node_name(n));
	{ INDENT
		ret = n->_vt->stop ? n->_vt->stop(n) : -1;
	}
	
	if (ret == 0)
		n->state = STATE_STOPPED;

	return ret;
}

int node_destroy(struct node *n)
{
	assert(n->state != STATE_DESTROYED && n->state != STATE_STARTED);

	if (n->_vt->destroy)
		n->_vt->destroy(n);
	
	list_remove(&n->_vt->instances, n);

	if (n->_vd)
		free(n->_vd);
	
	if (n->_name)
		free(n->_name);
	
	n->state = STATE_DESTROYED;
	
	return 0;
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
	
	for (int i = 0; i < nread; i++)
		smps[i]->source = n;
	
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

char * node_name(struct node *n)
{
	if (!n->_name)
		strcatf(&n->_name, RED("%s") "(" YEL("%s") ")", n->name, plugin_name(n->_vt));
		
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

int node_reverse(struct node *n)
{
	return n->_vt->reverse ? n->_vt->reverse(n) : -1;
}

int node_parse_list(struct list *list, config_setting_t *cfg, struct list *all)
{
	const char *str;
	struct node *node;

	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_STRING:
			str = config_setting_get_string(cfg);
			if (str) {
				node = list_lookup(all, str);
				if (node)
					list_push(list, node);
				else
					cerror(cfg, "Unknown outgoing node '%s'", str);
			}
			else
				cerror(cfg, "Invalid outgoing node");
			break;

		case CONFIG_TYPE_ARRAY:
			for (int i = 0; i < config_setting_length(cfg); i++) {
				config_setting_t *elm = config_setting_get_elem(cfg, i);
				
				str = config_setting_get_string(elm);
				if (str) {
					node = list_lookup(all, str);
					if (!node)
						cerror(elm, "Unknown outgoing node '%s'", str);
					else if (node->_vt->write == NULL)
						cerror(cfg, "Output node '%s' is not supported as a sink.", node_name(node));

					list_push(list, node);
				}
				else
					cerror(cfg, "Invalid outgoing node");
			}
			break;

		default:
			cerror(cfg, "Invalid output node(s)");
	}

	return list_length(list);
}
