/**
 * Message paths
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "cfg.h"
#include "utils.h"
#include "path.h"

extern struct config config;

int path_create(struct path *p, struct node *in, struct node *out)
{
	memset(p, 0, sizeof(struct path));

	p->in = in;
	p->out = out;

	return 0;
}

static void * path_run(void *arg)
{
	struct path *p = (struct path *) arg;
	struct msg m;

	/* main thread loop */
	while (1) {
		/* Receive message */
		msg_recv(&m, p->in);

		/* Check message sequence number */
		if (m.sequence < p->sequence) {
			p->delayed++;
			continue;
		}
		else if (m.sequence == p->sequence) {
			p->duplicated++;
		}

		p->received++;


		/* Call hooks */
		for (int i = 0; i < MAX_HOOKS && p->hooks[i]; i++) {
			p->hooks[i](&m);
		}

		/* Send message */
		msg_send(&m, p->out);
	}

	return NULL;
}

int path_start(struct path *p)
{
	pthread_create(&p->tid, NULL, &path_run, (void *) p);
}

int path_stop(struct path *p)
{
	void *ret;

	pthread_cancel(p->tid);
	pthread_join(p->tid, &ret);

	return 0;
}
