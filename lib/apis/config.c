/** The "config" command
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <libconfig.h>

#include "api.h"
#include "utils.h"
#include "plugin.h"

static int api_config(struct api_ressource *h, json_t *args, json_t **resp, struct api_session *s)
{
	config_setting_t *cfg_root = config_root_setting(&s->api->super_node->cfg);
	
	if (cfg_root) {
		*resp = json_pack("{ s: o }",
			"config", config_to_json(cfg_root));
	}
	/* No configuration has been loaded yet. */
	else {
		*resp = json_pack("{ s: s, s: i }",
				"error", "no configuration loaded",
				"code", 2);
	}
	
	return 0;
}

static struct plugin p = {
	.name = "config",
	.description = "retrieve current VILLASnode configuration",
	.type = PLUGIN_TYPE_API,
	.api.cb = api_config
};

REGISTER_PLUGIN(&p)