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

#include "timing.h"
#include "config.h"
#include "io/msg.h"
#include "hook.h"
#include "path.h"
#include "utils.h"
#include "node.h"
#include "plugin.h"
#include "config_helper.h"

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

int hook_parse(struct hook *h, json_t *cfg)
{
	int ret;
	json_error_t err;

	assert(h->state != STATE_DESTROYED);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i }",
		"priority", &h->priority
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", plugin_name(h->_vt));

	ret = h->_vt->parse ? h->_vt->parse(h, cfg) : 0;
	if (ret)
		return ret;

	h->cfg = cfg;
	h->state = STATE_PARSED;

	return 0;
}

int hook_parse_cli(struct hook *h, int argc, char *argv[])
{
	int ret;

	if (h->_vt->parse_cli) {
		ret = h->_vt->parse_cli(h, argc, argv);
		if (ret)
			return ret;

		h->state = STATE_PARSED;
	}
	else {
		h->cfg = json_load_cli(argc, argv);
		if (!h->cfg)
			return -1;

		ret = hook_parse(h, h->cfg);
	}

	return ret;
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

int hook_read(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	debug(LOG_HOOK | 10, "Running hook %s: type=read, priority=%d, cnt=%u", plugin_name(h->_vt), h->priority, *cnt);

	return h->_vt->read ? h->_vt->read(h, smps, cnt) : 0;
}

int hook_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	debug(LOG_HOOK | 10, "Running hook %s: type=process, priority=%d, cnt=%u", plugin_name(h->_vt), h->priority, *cnt);

	return h->_vt->process ? h->_vt->process(h, smps, cnt) : 0;
}

int hook_write(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	debug(LOG_HOOK | 10, "Running hook %s: type=write, priority=%d, cnt=%u", plugin_name(h->_vt), h->priority, *cnt);

	return h->_vt->write ? h->_vt->write(h, smps, cnt) : 0;
}

static int hook_run_list(struct list *hs, struct sample *smps[], unsigned cnt, int (*func)(struct hook *, struct sample **, unsigned *))
{
	unsigned ret;

	for (size_t i = 0; i < list_length(hs); i++) {
		struct hook *h = list_at(hs, i);

		ret = func(h, smps, &cnt);
		if (ret || !cnt)
			/* Abort hook processing if earlier hooks removed all samples
			 * or they returned something non-zero */
			break;
	}

	return cnt;
}

int hook_read_list(struct list *hs, struct sample *smps[], unsigned cnt)
{
	return hook_run_list(hs, smps, cnt, hook_read);
}

int hook_process_list(struct list *hs, struct sample *smps[], unsigned cnt)
{
	return hook_run_list(hs, smps, cnt, hook_process);
}

int hook_write_list(struct list *hs, struct sample *smps[], unsigned cnt)
{
	return hook_run_list(hs, smps, cnt, hook_write);
}

int hook_cmp_priority(const void *a, const void *b)
{
	struct hook *ha = (struct hook *) a;
	struct hook *hb = (struct hook *) b;

	return ha->priority - hb->priority;
}

int hook_parse_list(struct list *list, json_t *cfg, struct path *o)
{
	if (!json_is_array(cfg))
		error("Hooks must be configured as a list of objects");

	size_t index;
	json_t *cfg_hook;
	json_array_foreach(cfg, index, cfg_hook) {
		int ret;
		const char *type;
		struct plugin *p;
		json_error_t err;

		ret = json_unpack_ex(cfg_hook, &err, 0, "{ s: s }", "type", &type);
		if (ret)
			jerror(&err, "Failed to parse hook");

		p = plugin_lookup(PLUGIN_TYPE_HOOK, type);
		if (!p)
			jerror(&err, "Unkown hook type '%s'", type);

		struct hook *h = alloc(sizeof(struct hook));

		ret = hook_init(h, &p->hook, o);
		if (ret)
			jerror(&err, "Failed to initialize hook");

		ret = hook_parse(h, cfg_hook);
		if (ret)
			jerror(&err, "Failed to parse hook configuration");

		list_push(list, h);
	}

	return 0;
}
