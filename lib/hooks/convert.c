/** Convert hook.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

/** @addtogroup hooks Hook functions
 * @{
 */

#include "hook.h"
#include "plugin.h"

struct convert {
	enum {
		TO_FIXED,
		TO_FLOAT
	} mode;
};

static int convert_parse(struct hook *h, config_setting_t *cfg)
{
	struct convert *p = h->_vd;
	
	const char *mode;
	
	if (!config_setting_lookup_string(cfg, "mode", &mode))
		cerror(cfg, "Missing setting 'mode' for hook '%s'", plugin_name(h->_vt));
	
	if      (!strcmp(mode, "fixed"))
		p->mode = TO_FIXED;
	else if (!strcmp(mode, "float"))
		p->mode = TO_FLOAT;
	else
		error("Invalid parameter '%s' for hook 'convert'", mode);
	
	return 0;
}

static int convert_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	struct convert *p = h->_vd;

	for (int i = 0; i < *cnt; i++) {
		for (int k = 0; k < smps[i]->length; k++) {
			switch (p->mode) {
				case TO_FIXED: smps[i]->data[k].i = smps[i]->data[k].f * 1e3; break;
				case TO_FLOAT: smps[i]->data[k].f = smps[i]->data[k].i; break;
			}
		}
	}

	return 0;
}

static struct plugin p = {
	.name		= "convert",
	.description	= "Convert message from / to floating-point / integer",
	.type		= PLUGIN_TYPE_HOOK,
	.hook		= {
		.priority = 99,
		.parse	= convert_parse,
		.read	= convert_read,
		.size	= sizeof(struct convert)
	}
};

REGISTER_PLUGIN(&p)

/** @} */