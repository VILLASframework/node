/** Nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <string.h>

#include "sample.h"
#include "node.h"
#include "super_node.h"
#include "utils.h"
#include "config.h"
#include "plugin.h"

int node_type_start(struct node_type *vt, struct super_node *sn)
{
	int ret;
	
	if (vt->state != STATE_STARTED)
		return 0;

	info("Initializing " YEL("%s") " node type which is used by %zu nodes", plugin_name(vt), list_length(&vt->instances));
	{ INDENT
		ret = vt->init ? vt->init(sn) : 0;
	}	

	if (ret == 0)
		vt->state = STATE_STARTED;

	return ret;
}

int node_type_stop(struct node_type *vt)
{
	int ret;
	
	if (vt->state != STATE_STARTED)
		return 0;

	info("De-initializing " YEL("%s") " node type", plugin_name(vt));
	{ INDENT
		ret = vt->deinit ? vt->deinit() : -1;
	}
	
	if (ret == 0)
		vt->state = STATE_DESTROYED;

	return ret;
}
