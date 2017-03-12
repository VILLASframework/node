/** Hook-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <string.h>
#include <math.h>
#include <libconfig.h>

#include "timing.h"
#include "config.h"
#include "msg.h"
#include "hook.h"
#include "path.h"
#include "utils.h"
#include "node.h"
#include "plugin.h"

int hook_init(struct hook *h, struct cfg *cfg)
{
	struct hook_info i = {
		.nodes = &cfg->nodes,
		.paths = &cfg->paths
	};
	
	if (h->type & HOOK_INIT)
		return h->cb(h, HOOK_INIT, &i);
	else
		return 0;
}

int hook_destroy(struct hook *h)
{
	struct hook_info i = { NULL };
	
	if (h->type & HOOK_DESTROY)
		return h->cb(h, HOOK_DESTROY, &i);
	else
		return 0;
}

int hook_copy(struct hook *h, struct hook *c)
{
	memcpy(c, h, sizeof(struct hook));
	
	c->_vd = 
	c->prev = 
	c->last = NULL;
	
	return 0;
}

int hook_cmp_priority(const void *a, const void *b) {
	struct hook *ha = (struct hook *) a;
	struct hook *hb = (struct hook *) b;
	
	return ha->priority - hb->priority;
}

int hook_run(struct path *p, struct sample *smps[], size_t cnt, int when)
{
	struct hook_info i = {
		.path = p,
		.smps = smps,
		.cnt = cnt
	};
	
	list_foreach(struct hook *h, &p->hooks) {
		if (h->type & when) {
			cnt = h->cb(h, when, &i);
			
			debug(LOG_HOOK | 22, "Ran hook '%s' when=%u prio=%u, cnt=%zu", plugin_name(h), when, h->priority, cnt);
			
			if (cnt == 0)
				break;
		}
	}

	return cnt;
}

void * hook_storage(struct hook *h, int when, size_t len, ctor_cb_t ctor, dtor_cb_t dtor)
{
	switch (when) {
		case HOOK_INIT:
			h->_vd = alloc(len);
			
			if (ctor)
				ctor(h->_vd);
			
			break;
			
		case HOOK_DESTROY:
			if (dtor)
				dtor(h->_vd);
		
			free(h->_vd);
			h->_vd = NULL;
			break;
	}
	
	return h->_vd;
}

/** Parse an array or single hook function.
 *
 * Examples:
 *     hooks = [ "print", "fir" ]
 *     hooks = "log"
 */
int hook_parse_list(struct list *list, config_setting_t *cfg)
{
	switch (config_setting_type(cfg)) {
		case CONFIG_TYPE_STRING:
			hook_parse(cfg, list);
			break;

		case CONFIG_TYPE_ARRAY:
			for (int i = 0; i < config_setting_length(cfg); i++)
				hook_parse(config_setting_get_elem(cfg, i), list);
			break;

		default:
			cerror(cfg, "Invalid hook functions");
	}

	return list_length(list);
}

int hook_parse(config_setting_t *cfg, struct list *list)
{
	struct hook *hook;
	struct plugin *plg;

	char *name, *param;
	const char *hookline = config_setting_get_string(cfg);
	if (!hookline)
		cerror(cfg, "Invalid hook function");
	
	name  = strtok((char *) hookline, ":");
	param = strtok(NULL, "");

	plg = plugin_lookup(PLUGIN_TYPE_HOOK, name);
	if (!plg)
		cerror(cfg, "Unknown hook function '%s'", name);
	
	hook = memdup(&plg->hook, sizeof(plg->hook));
	hook->parameter = param;
	
	list_push(list, hook);

	return 0;
}