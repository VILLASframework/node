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

#include <villas/timing.h>
#include <villas/config.h>
#include <villas/hook.h>
#include <villas/path.h>
#include <villas/utils.h>
#include <villas/node.h>
#include <villas/plugin.h>
#include <villas/config_helper.h>

int hook_init(struct hook *h, struct hook_type *vt, struct path *p, struct node *n)
{
	int ret;

	assert(h->state == STATE_DESTROYED);

	h->priority = vt->priority;

	/* Node hooks can only used with nodes,
	   Path hooks only with paths.. */
	if ((!(vt->flags & HOOK_NODE_READ) && n) ||
	    (!(vt->flags & HOOK_NODE_WRITE) && n) ||
	    (!(vt->flags & HOOK_PATH) && p))
		return -1;

	h->path = p;
	h->node = n;

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
	if (h->_vt->start) {
		debug(LOG_HOOK | 10, "Start hook %s: priority=%d", plugin_name(h->_vt), h->priority);

		return h->_vt->start(h);
	}
	else
		return 0;
}

int hook_stop(struct hook *h)
{
	if (h->_vt->stop) {
		debug(LOG_HOOK | 10, "Stopping hook %s: priority=%d", plugin_name(h->_vt), h->priority);

		return h->_vt->stop(h);
	}
	else
		return 0;
}

int hook_periodic(struct hook *h)
{
	if (h->_vt->periodic) {
		debug(LOG_HOOK | 10, "Periodic hook %s: priority=%d", plugin_name(h->_vt), h->priority);

		return h->_vt->periodic(h);
	}
	else
		return 0;
}

int hook_restart(struct hook *h)
{
	if (h->_vt->restart) {
		debug(LOG_HOOK | 10, "Restarting hook %s: priority=%d", plugin_name(h->_vt), h->priority);

		return h->_vt->restart(h);
	}
	else
		return 0;
}

int hook_process(struct hook *h, struct sample *smps[], unsigned *cnt)
{
	if (h->_vt->process) {
		debug(LOG_HOOK | 10, "Process hook %s: priority=%d, cnt=%d", plugin_name(h->_vt), h->priority, *cnt);

		return h->_vt->process(h, smps, cnt);
	}
	else
		return 0;
}

int hook_process_list(struct list *hs, struct sample *smps[], unsigned cnt)
{
	unsigned ret;

	for (size_t i = 0; i < list_length(hs); i++) {
		struct hook *h = (struct hook *) list_at(hs, i);

		ret = hook_process(h, smps, &cnt);
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

int hook_parse_list(struct list *list, json_t *cfg, int mask, struct path *o, struct node *n)
{
	if (!json_is_array(cfg))
		error("Hooks must be configured as a list of objects");

	size_t i;
	json_t *json_hook;
	json_array_foreach(cfg, i, json_hook) {
		int ret;
		const char *type;
		struct hook_type *ht;
		json_error_t err;

		ret = json_unpack_ex(json_hook, &err, 0, "{ s: s }", "type", &type);
		if (ret)
			jerror(&err, "Failed to parse hook");

		ht = hook_type_lookup(type);
		if (!ht)
			jerror(&err, "Unkown hook type '%s'", type);

		if (!(ht->flags & mask))
			error("Hook %s not allowed here.", type);

		struct hook *h = (struct hook *) alloc(sizeof(struct hook));

		ret = hook_init(h, ht, o, n);
		if (ret)
			error("Failed to initialize hook: %s", type);

		ret = hook_parse(h, json_hook);
		if (ret)
			jerror(&err, "Failed to parse hook configuration");

		list_push(list, h);
	}

	return 0;
}

int hook_init_builtin_list(struct list *l, bool builtin, int mask, struct path *p, struct node *n)
{
	int ret;

	ret = list_init(l);
	if (ret)
		return ret;

	if (!builtin)
		return 0;

	for (size_t i = 0; i < list_length(&plugins); i++) {
		struct plugin *q = (struct plugin *) list_at(&plugins, i);

		struct hook *h;
		struct hook_type *vt = &q->hook;

		if (q->type != PLUGIN_TYPE_HOOK)
			continue;

		if (vt->flags & mask)
			continue;

		h = (struct hook *) alloc(sizeof(struct hook));
		if (!h)
			return -1;

		ret = hook_init(h, vt, p, n);
		if (ret)
			return ret;

		list_push(l, h);
	}

	return 0;
}
