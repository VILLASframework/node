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

#include <villas/plugin.hpp>
#include <villas/hook.hpp>
#include <villas/hook_list.h>
#include <villas/list.h>
#include <villas/log.h>

using namespace villas;
using namespace villas::node;

extern "C" {

int hook_list_init(vlist *hs)
{
	int ret;

	ret = vlist_init(hs);
	if (ret)
		return ret;

	return 0;
}

static int hook_destroy(Hook *h)
{
	delete h;

	return 0;
}

int hook_list_destroy(vlist *hs)
{
	int ret;

	ret = vlist_destroy(hs, (dtor_cb_t) hook_destroy, false);
	if (ret)
		return ret;

	return 0;
}

int hook_list_parse(vlist *hs, json_t *cfg, int mask, struct path *o, struct node *n)
{
	if (!json_is_array(cfg))
		//throw ConfigError(cfg, "node-config-hook", "Hooks must be configured as a list of hook objects");
		return -1;

	size_t i;
	json_t *json_hook;
	json_array_foreach(cfg, i, json_hook) {
		int ret;
		const char *type;
		Hook *h;
		json_error_t err;

		ret = json_unpack_ex(json_hook, &err, 0, "{ s: s }", "type", &type);
		if (ret)
			throw ConfigError(json_hook, err, "node-config-hook", "Failed to parse hook");

		auto hf = plugin::Registry::lookup<HookFactory>(type);
		if (!hf)
			throw ConfigError(json_hook, "node-config-hook", "Unkown hook type '{}'", type);

		if (!(hf->getFlags() & mask))
			throw ConfigError(json_hook, "node-config-hook", "Hook '{}' not allowed here", type);

		try {
			h = hf->make(o, n);
			h->parse(json_hook);
			h->check();
		}
		catch (...) {
			return -1;
		}

		vlist_push(hs, h);
	}

	return 0;
}

static int hook_cmp_priority(const Hook *a, const Hook *b)
{
	return a->getPriority() - b->getPriority();
}

int hook_list_prepare(vlist *hs, vlist *sigs, int m, struct path *p, struct node *n)
{
	assert(hs->state == STATE_INITIALIZED);

	/* Add internal hooks if they are not already in the list */
	for (auto f : plugin::Registry::lookup<HookFactory>()) {
		if ((f->getFlags() & m) == m) {
			auto h = f->make(p, n);

			vlist_push(hs, h);
		}
	}

	/* We sort the hooks according to their priority */
	vlist_sort(hs, (cmp_cb_t) hook_cmp_priority);

	for (size_t i = 0; i < vlist_length(hs); i++) {
		Hook *h = (Hook *) vlist_at(hs, i);

		if (!h->isEnabled())
			continue;

		try {
			h->prepare(sigs);
		} catch (...) {
			return -1;
		}

		sigs = h->getSignals();
	}

	return 0;
}

int hook_list_process(vlist *hs, sample *smps[], unsigned cnt)
{
	unsigned ret, curent, processed = 0;

	if (vlist_length(hs) == 0)
		return cnt;

	for (curent = 0; curent < cnt; curent++) {
		sample *smp = smps[curent];

		for (size_t i = 0; i < vlist_length(hs); i++) {
			Hook *h = (Hook *) vlist_at(hs, i);

			if (!h->isEnabled())
				continue;

			ret = h->process(smp);
			switch (ret) {
				case HOOK_ERROR:
					return -1;

				case HOOK_OK:
					continue;

				case HOOK_SKIP_SAMPLE:
					goto skip;

				case HOOK_STOP_PROCESSING:
					goto stop;
			}
		}

		smps[processed++] = smp;
skip: {}
	}

stop:	return processed;
}

int hook_list_periodic(vlist *hs)
{
	for (size_t j = 0; j < vlist_length(hs); j++) {
		Hook *h = (Hook *) vlist_at(hs, j);

		if (!h->isEnabled())
			continue;

		try {
			h->periodic();
		} catch (...) {
			return -1;
		}
	}

	return 0;
}

int hook_list_start(vlist *hs)
{
	for (size_t i = 0; i < vlist_length(hs); i++) {
		Hook *h = (Hook *) vlist_at(hs, i);

		if (!h->isEnabled())
			continue;

		try {
			h->start();
		} catch (...) {
			return -1;
		}
	}

	return 0;
}

int hook_list_stop(vlist *hs)
{
	for (size_t i = 0; i < vlist_length(hs); i++) {
		Hook *h = (Hook *) vlist_at(hs, i);

		try {
			h->stop();
		} catch (...) {
			return -1;
		}
	}

	return 0;
}

vlist * hook_list_get_signals(vlist *hs)
{
	Hook *h = (Hook *) vlist_last(hs);

	return h->getSignals();
}

}
