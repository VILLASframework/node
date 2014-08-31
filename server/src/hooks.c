/** Hook funktions
 *
 * Every path can register a hook function which is called for every received
 * message. This can be used to debug the data flow, get statistics
 * or alter the message.
 *
 * This file includes some examples.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */
 
#include <string.h>

#include "msg.h"
#include "hooks.h"

/** @todo Make const */
static struct hook_id hook_list[] = {
	{ hook_print, "print" },
	{ hook_filter, "filter" },
	{ hook_tofixed, "tofixed" },
	{ hook_multiple, "multiple" },
	{ NULL }
};

hook_cb_t hook_lookup(const char *name)
{
	for (struct hook_id *hid = hook_list; hid->cb; hid++) {
		if (!strcmp(name, hid->name)) {
			return hid->cb;
		}
	}

	return NULL; /* No matching hook was found */
}
 
int hook_print(struct msg *m, struct path *p)
{
	/* Print every message once to stdout */
	msg_fprint(stdout, m);

	return 0;
}

int hook_filter(struct msg *m, struct path *p)
{
	/* Drop every 10th message */
	if (m->sequence % 10 == 0)
		return -1;
	else
		return 0;
}

int hook_tofixed(struct msg *m, struct path *p)
{
	for (int i=0; i<m->length; i++) {
		m->data[i].i = m->data[i].f * 1e3; 
	}
	
	return 0;
}

int hook_multiple(struct msg *m, struct path *p)
{
	if (hook_print(m, p))
		return -1;
	else if (hook_tofixed(m, p))
		return -1;
	else
		return 0;
}
