/** The "restart" API ressource.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include "plugin.h"
#include "api.h"

/** @todo not implemented yet */
static int api_restart(struct api_action *h, json_t *args, json_t **resp, struct api_session *s)
{
	return -1;
}

static struct plugin p = {
	.name = "restart",
	.description = "restart VILLASnode with new configuration",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_restart
};

REGISTER_PLUGIN(&p)