/** Hook-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/plugin.h>
#include <villas/hook.h>
#include <villas/hook_type.h>
#include <villas/hook_list.h>
#include <villas/list.h>
#include <villas/log.h>

extern "C" {

int hook_list_init(vlist *hs)
{
	int ret;

	ret = vlist_init(hs);
	if (ret)
		return ret;

	return 0;
}

int hook_list_destroy(vlist *hs)
{
	int ret;

	ret = vlist_destroy(hs, (dtor_cb_t) hook_destroy, true);
	if (ret)
		return ret;

	return 0;
}

int hook_list_parse(vlist *hs, json_t *cfg, int mask, path *o, node *n)
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

		hook *h = (hook *) alloc(sizeof(hook));

		ret = hook_init(h, ht, o, n);
		if (ret)
			error("Failed to initialize hook: %s", type);

		ret = hook_parse(h, json_hook);
		if (ret)
			jerror(&err, "Failed to parse hook configuration");

		vlist_push(hs, h);
	}

	return 0;
}

int hook_list_prepare(vlist *hs, vlist *sigs, int m, path *p, node *n)
{
	int ret;

	/* Add internal hooks if they are not already in the list */
	ret = hook_list_add(hs, m, p, n);
	if (ret)
		return ret;

	/* We sort the hooks according to their priority */
	vlist_sort(hs, hook_cmp_priority);

	for (size_t i = 0; i < vlist_length(hs); i++) {
		hook *h = (hook *) vlist_at(hs, i);

		ret = hook_prepare(h, sigs);
		if (ret)
			return ret;

		sigs = &h->signals;
	}

	return 0;
}

int hook_list_add(vlist *hs, int mask, path *p, node *n)
{
	int ret;

	assert(hs->state == STATE_INITIALIZED);

	for (size_t i = 0; i < vlist_length(&plugins); i++) {
		plugin *q = (plugin *) vlist_at(&plugins, i);

		hook *h;
		struct hook_type *ht = &q->hook;

		if (q->type != PLUGIN_TYPE_HOOK)
			continue;

		if ((ht->flags & mask) == mask) {
			h = (hook *) alloc(sizeof(hook));
			if (!h)
				return -1;

			ret = hook_init(h, ht, p, n);
			if (ret)
				return ret;

			vlist_push(hs, h);
		}
	}

	return 0;
}

int hook_list_process(vlist *hs, sample *smps[], unsigned cnt)
{
	unsigned ret, curent, processed = 0;

	for (curent = 0; curent < cnt; curent++) {
		sample *smp = smps[curent];

		for (size_t i = 0; i < vlist_length(hs); i++) {
			hook *h = (hook *) vlist_at(hs, i);

			ret = hook_process(h, smp);
			switch (ret) {
				case HOOK_ERROR:
					return -1;

				case HOOK_OK:
					smps[processed++] = smp;
					break;

				case HOOK_SKIP_SAMPLE:
					goto skip;

				case HOOK_STOP_PROCESSING:
					goto stop;
			}
		}
skip: {}
	}

stop:	return processed;
}

int hook_list_periodic(vlist *hs)
{
	int ret;

	for (size_t j = 0; j < vlist_length(hs); j++) {
		hook *h = (hook *) vlist_at(hs, j);

		ret = hook_periodic(h);
		if (ret)
			return ret;
	}

	return 0;
}

int hook_list_start(vlist *hs)
{
	int ret;

	for (size_t i = 0; i < vlist_length(hs); i++) {
		hook *h = (hook *) vlist_at(hs, i);

		ret = hook_start(h);
		if (ret)
			return ret;
	}

	return 0;
}

int hook_list_stop(vlist *hs)
{
	int ret;

	for (size_t i = 0; i < vlist_length(hs); i++) {
		hook *h = (hook *) vlist_at(hs, i);

		ret = hook_stop(h);
		if (ret)
			return ret;
	}

	return 0;
}

vlist * hook_list_get_signals(vlist *hs)
{
	hook *h = (hook *) vlist_last(hs);

	return &h->signals;
}

}
