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

#include <string.h>
#include <math.h>

#include <villas/timing.h>
#include <villas/node/config.h>
#include <villas/hook.h>
#include <villas/path.h>
#include <villas/utils.h>
#include <villas/node.h>
#include <villas/plugin.h>

const char *hook_reasons[] = {
	"ok", "error", "skip-sample", "stop-processing"
};

int hook_init(struct hook *h, struct hook_type *vt, struct path *p, struct node *n)
{
	int ret;

	assert(h->state == STATE_DESTROYED);

	h->enabled = 1;
	h->priority = vt->priority;
	h->path = p;
	h->node = n;

	h->signals.state = STATE_DESTROYED;

	ret = signal_list_init(&h->signals);
	if (ret)
		return ret;

	h->_vt = vt;
	h->_vd = alloc(vt->size);

	ret = hook_type(h)->init ? hook_type(h)->init(h) : 0;
	if (ret)
		return ret;

	// We dont need to parse builtin hooks
	h->state = hook_type(h)->flags & HOOK_BUILTIN ? STATE_PARSED : STATE_INITIALIZED;

	return 0;
}

int hook_prepare(struct hook *h, struct vlist *signals)
{
	int ret;

	assert(h->state == STATE_PARSED);

	if (!h->enabled)
		return 0;

	ret = signal_list_copy(&h->signals, signals);
	if (ret)
		return -1;

	ret = hook_type(h)->init_signals ? hook_type(h)->init_signals(h) : 0;
	if (ret)
		return ret;

	h->state = STATE_PREPARED;

	return 0;
}

int hook_parse(struct hook *h, json_t *cfg)
{
	int ret;
	json_error_t err;

	assert(h->state != STATE_DESTROYED);

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: i, s?: b }",
		"priority", &h->priority,
		"enabled", &h->enabled
	);
	if (ret)
		jerror(&err, "Failed to parse configuration of hook '%s'", hook_type_name(hook_type(h)));

	ret = hook_type(h)->parse ? hook_type(h)->parse(h, cfg) : 0;
	if (ret)
		return ret;

	h->cfg = cfg;
	h->state = STATE_PARSED;

	return 0;
}

int hook_destroy(struct hook *h)
{
	int ret;

	assert(h->state != STATE_DESTROYED && h->state != STATE_STARTED);

	ret = signal_list_destroy(&h->signals);
	if (ret)
		return ret;

	ret = hook_type(h)->destroy ? hook_type(h)->destroy(h) : 0;
	if (ret)
		return ret;

	if (h->_vd)
		free(h->_vd);

	h->state = STATE_DESTROYED;

	return 0;
}

int hook_start(struct hook *h)
{
	int ret;
	assert(h->state == STATE_PREPARED);

	if (!h->enabled)
		return 0;

	debug(LOG_HOOK | 10, "Start hook %s: priority=%d", hook_type_name(hook_type(h)), h->priority);

	ret = hook_type(h)->start ? hook_type(h)->start(h) : 0;
	if (ret)
		return ret;

	h->state = STATE_STARTED;

	return 0;
}

int hook_stop(struct hook *h)
{
	int ret;
	assert(h->state == STATE_STARTED);

	if (!h->enabled)
		return 0;

	debug(LOG_HOOK | 10, "Stopping hook %s: priority=%d", hook_type_name(hook_type(h)), h->priority);

	ret = hook_type(h)->stop ? hook_type(h)->stop(h) : 0;
	if (ret)
		return ret;

	h->state = STATE_STOPPED;

	return 0;
}

int hook_periodic(struct hook *h)
{
	assert(h->state == STATE_STARTED);

	if (!h->enabled)
		return 0;

	if (hook_type(h)->periodic) {
		debug(LOG_HOOK | 10, "Periodic hook %s: priority=%d", hook_type_name(hook_type(h)), h->priority);

		return hook_type(h)->periodic(h);
	}
	else
		return 0;
}

int hook_restart(struct hook *h)
{
	int ret;
	assert(h->state == STATE_STARTED);

	if (!h->enabled)
		return 0;

	debug(LOG_HOOK | 10, "Restarting hook %s: priority=%d", hook_type_name(hook_type(h)), h->priority);

	ret = hook_type(h)->restart ? hook_type(h)->restart(h) : 0;
	if (ret)
		return ret;

	return 0;
}

int hook_process(struct hook *h, struct sample *smp)
{
	int ret;
	assert(h->state == STATE_STARTED);

	if (!h->enabled)
		return 0;

	ret = hook_type(h)->process ? hook_type(h)->process(h, smp) : 0;
	if (ret)
		return ret;

	debug(LOG_HOOK | 10, "Hook %s processed: priority=%d, return=%s", hook_type_name(hook_type(h)), h->priority, hook_reasons[ret]);

	return 0;
}

int hook_cmp_priority(const void *a, const void *b)
{
	struct hook *ha = (struct hook *) a;
	struct hook *hb = (struct hook *) b;

	return ha->priority - hb->priority;
}

const char * hook_type_name(struct hook_type *vt)
{
	return plugin_name(vt);
}
