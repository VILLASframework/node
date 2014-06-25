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

/**
 * @brief This is the main thread function per path
 */
static void * path_run(void *arg)
{
	struct path *p = (struct path *) arg;
	struct msg m;

	/* main thread loop */
	while (1) {
		msg_recv(&m, p->in); /* Receive message */

		/* Check message sequence number */
		if (m.sequence < p->sequence) {
			p->delayed++;

			/* Delayed messages will be skipped */
			continue;
		}
		else if (m.sequence == p->sequence) {
			p->duplicated++;
		}

		p->sequence = m.sequence;
		p->received++;

		/* Call hook */
		if (p->hook && p->hook(&m)) {
			/* The hook can act as a simple filter
			 * Returning a non-zero value will skip
			 * the message from being forwarded */
			continue;
		}

		msg_send(&m, p->out); /* Send message */
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
