/** Hook-releated functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <string.h>
#include <math.h>

#include "timing.h"
#include "config.h"
#include "msg.h"
#include "hooks.h"
#include "path.h"
#include "utils.h"
#include "node.h"

struct list hooks;

int hook_init(struct hook *h, struct list *nodes, struct list *paths, struct settings *settings)
{
	struct hook_info i = {
		.paths = paths,
		.nodes = nodes,
		.settings = settings
	};
	
	return h->cb(h, HOOK_INIT, &i);
}

void hook_destroy(struct hook *h)
{
	struct hook_info i = { NULL };
	h->cb(h, HOOK_DESTROY, &i);
}

int hook_copy(struct hook *h, struct hook *c)
{
	memcpy(c, h, sizeof(struct hook));
	
	c->_vd = 
	c->prev = 
	c->last = NULL;
	
	return 0;
}

int hooks_sort_priority(const void *a, const void *b) {
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
			debug(DBG_HOOK | 22, "Running hook when=%u '%s' prio=%u, cnt=%zu", when, h->name, h->priority, cnt);

			cnt = h->cb(h, when, &i);
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