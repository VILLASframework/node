/** Hook-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

int hook_init(struct hook *h, struct hook_type *vt, struct path *p)
{
	int ret;

	assert(h->state == STATE_DESTROYED);

	h->priority = vt->priority;
	h->path = p;

	h->_vt = vt;
	h->_vd = alloc(vt->size);

	ret = h->_vt->init ? h->_vt->init(h) : 0;
	if (ret)
		return ret;

	h->state = STATE_INITIALIZED;

	return 0;
}

int hook_parse(struct hook *h, config_setting_t *cfg)
{
	int ret;

	assert(h->state != STATE_DESTROYED);

	config_setting_lookup_int(cfg, "priority", &h->priority);

	ret = h->_vt->parse ? h->_vt->parse(h, cfg) : 0;
	if (ret)
		return ret;

	h->state = STATE_PARSED;

	return 0;
}

int hook_destroy(struct hook *h)
{
	int ret;

	assert(h->state != STATE_DESTROYED);

	ret = h->_vt->destroy ? h->_vt->destroy(h) : 0;
	if (ret)
		return ret;

	if (h->_vd)
		free(h->_vd);

	h->state = STATE_DESTROYED;

	return 0;
}

int hook_start(struct hook *h)
{
	debug(LOG_HOOK | 10, "Running hook %s: type=sart, priority=%d", plugin_name(h->_vt), h->priority);

	return h->_vt->start ? h->_vt->start(h) : 0;
}

int hook_stop(struct hook *h)
{
	debug(LOG_HOOK | 10, "Running hook %s: type=stop, priority=%d", plugin_name(h->_vt), h->priority);

	return h->_vt->stop ? h->_vt->stop(h) : 0;
}

int hook_periodic(struct hook *h)
{
	debug(LOG_HOOK | 10, "Running hook %s: type=periodic, priority=%d", plugin_name(h->_vt), h->priority);

	return h->_vt->periodic ? h->_vt->periodic(h) : 0;
}

int hook_restart(struct hook *h)
{
	debug(LOG_HOOK | 10, "Running hook %s: type=restart, priority=%d", plugin_name(h->_vt), h->priority);
	
	return h->_vt->restart ? h->_vt->restart(h) : 0;
}

int hook_read(struct hook *h, struct sample *smps[], size_t *cnt)
{
	debug(LOG_HOOK | 10, "Running hook %s: type=read, priority=%d, cnt=%zu", plugin_name(h->_vt), h->priority, *cnt);
	
	return h->_vt->read ? h->_vt->read(h, smps, cnt) : 0;
}

int hook_write(struct hook *h, struct sample *smps[], size_t *cnt)
{
	debug(LOG_HOOK | 10, "Running hook %s: type=write, priority=%d, cnt=%zu", plugin_name(h->_vt), h->priority, *cnt);

	return h->_vt->write ? h->_vt->write(h, smps, cnt) : 0;
}

size_t hook_read_list(struct list *hs, struct sample *smps[], size_t cnt)
{
	size_t ret;

	for (size_t i = 0; i < list_length(hs); i++) {
		struct hook *h = list_at(hs, i);

		ret = hook_read(h, smps, &cnt);
		if (ret || !cnt)
			/* Abort hook processing if earlier hooks removed all samples
			 * or they returned something non-zero */
			break;
	}

	return cnt;
}

size_t hook_write_list(struct list *hs, struct sample *smps[], size_t cnt)
{
	size_t ret;

	for (size_t i = 0; i < list_length(hs); i++) {
		struct hook *h = list_at(hs, i);

		ret = hook_write(h, smps, &cnt);
		if (ret || !cnt)
			/* Abort hook processing if earlier hooks removed all samples
			 * or they returned something non-zero */
			break;
	}

	return cnt;
}

int hook_cmp_priority(const void *a, const void *b)
{
	struct hook *ha = (struct hook *) a;
	struct hook *hb = (struct hook *) b;

	return ha->priority - hb->priority;
}

int hook_parse_list(struct list *list, config_setting_t *cfg, struct path *o)
{
	struct plugin *p;

	int ret;
	const char *type;

	if (!config_setting_is_list(cfg))
		cerror(cfg, "Hooks must be configured as a list of objects");

	for (int i = 0; i < config_setting_length(cfg); i++) {
		config_setting_t *cfg_hook = config_setting_get_elem(cfg, i);

		if (!config_setting_is_group(cfg_hook))
			cerror(cfg_hook, "The 'hooks' setting must be an array of strings.");

		if (!config_setting_lookup_string(cfg_hook, "type", &type))
			cerror(cfg_hook, "Missing setting 'type' for hook");

		p = plugin_lookup(PLUGIN_TYPE_HOOK, type);
		if (!p)
			continue; /* We ignore all non hook settings in this libconfig object setting */

		struct hook *h = alloc(sizeof(struct hook));
		
		ret = hook_init(h, &p->hook, o);
		if (ret)
			cerror(cfg_hook, "Failed to initialize hook");

		ret = hook_parse(h, cfg_hook);
		if (ret)
			cerror(cfg_hook, "Failed to parse hook configuration");

		list_push(list, h);
	}

	return 0;
}
