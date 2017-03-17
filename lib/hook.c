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

int hook_init(struct hook *h, struct hook_type *vt, struct super_node *sn)
{
	int ret;

	struct hook_info i = {
		.nodes = &sn->nodes,
		.paths = &sn->paths
	};
	
	assert(h->state == STATE_DESTROYED);
	
	h->_vt = vt;
	h->priority = vt->priority;
	
	ret = hook_run(h, HOOK_INIT, &i);
	if (ret)
		return ret;
	
	h->state = STATE_INITIALIZED;
	
	return 0;
}

int hook_parse(struct hook *h, config_setting_t *cfg)
{
	int ret;

	assert(h->state != STATE_DESTROYED);
	
	h->cfg = cfg;
	
	config_setting_lookup_int(h->cfg, "priority", &h->priority);

	/* Parse hook arguments */
	ret = hook_run(h, HOOK_PARSE, NULL);
	if (ret)
		return ret;
	
	h->state = STATE_PARSED;
	
	return 0;
}

int hook_destroy(struct hook *h)
{
	int ret;

	assert(h->state != STATE_DESTROYED);
	
	ret = hook_run(h, HOOK_DESTROY, NULL);
	if (ret)
		return ret;
	
	h->state = HOOK_DESTROYED;
	
	return 0;
}

int hook_cmp_priority(const void *a, const void *b)
{
	struct hook *ha = (struct hook *) a;
	struct hook *hb = (struct hook *) b;
	
	return ha->priority - hb->priority;
}

int hook_run(struct hook *h, int when, struct hook_info *i)
{
	debug(LOG_HOOK | 22, "Running hook '%s' when=%u, prio=%u, cnt=%zu", plugin_name(h->_vt), when, h->priority, i ? i->count : 0);
	
	return h->_vt->when & when ? h->_vt->cb(h, when, i) : 0;
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

int hook_parse_list(struct list *list, config_setting_t *cfg, struct super_node *sn)
{
	struct hook h;
	struct plugin *p;

	int ret;

	if (!config_setting_is_group(cfg))
		cerror(cfg, "Hooks must be configured with an object");
	
	int priority = 10;
	for (int i = 0; i < config_setting_length(cfg); i++) {
		config_setting_t *cfg_hook = config_setting_get_elem(cfg, i);
		
		const char *name = config_setting_name(cfg_hook);
		
		p = plugin_lookup(PLUGIN_TYPE_HOOK, name);
		if (!p)
			continue; /* We ignore all non hook settings in this libconfig object setting */
		
		if (!config_setting_is_group(cfg_hook))
			cerror(cfg_hook, "The 'hooks' setting must be an array of strings.");
	
		ret = hook_init(&h, &p->hook, sn);
		if (ret)
			continue;
		
		h.priority = priority++;
		
		ret = hook_parse(&h, cfg_hook);
		if (ret)
			continue;

		list_push(list, memdup(&h, sizeof(h)));
	}

	return list_length(list);
}