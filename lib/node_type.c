/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <string.h>
#include <libconfig.h>

#include "sample.h"
#include "node.h"
#include "cfg.h"
#include "utils.h"
#include "config.h"
#include "plugin.h"

int node_type_init(struct node_type *vt, int argc, char *argv[], config_setting_t *cfg)
{
	int ret;
	
	if (vt->state != NODE_TYPE_DEINITIALIZED)
		return -1;

	info("Initializing " YEL("%s") " node type", plugin_name(vt));
	{ INDENT
		ret = vt->init ? vt->init(argc, argv, cfg) : 0;
	}	

	if (ret == 0)
		vt->state = NODE_TYPE_INITIALIZED;

	return ret;
}

int node_type_deinit(struct node_type *vt)
{
	int ret;
	
	if (vt->state != NODE_TYPE_INITIALIZED)
		return -1;

	info("De-initializing " YEL("%s") " node type", plugin_name(vt));
	{ INDENT
		ret = vt->deinit ? vt->deinit() : -1;
	}
	
	if (ret == 0)
		vt->state = NODE_TYPE_DEINITIALIZED;

	return ret;
}
