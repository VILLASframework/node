/** The "reload" command
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include "api.h"

static int api_reload(struct api_ressource *h, json_t *args, json_t **resp, struct api_info *i)
{
	return -1;
}

static struct plugin p = {
	.name = "reload",
	.description = "restart VILLASnode with new configuration",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_reload
};

REGISTER_PLUGIN(&p)