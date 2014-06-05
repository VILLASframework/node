/**
 * Message paths
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <string.h>
#include <stdlib.h>
#include <poll.h>
#include <errno.h>

#include "utils.h"
#include "path.h"

struct path* path_create(struct node *in, struct node *out)
{
	struct path *p = malloc(sizeof(struct path));
	if (!p)
		return NULL;

	memset(p, 0, sizeof(struct path));

	p->in = in;
	p->out = out;

	return p;
}

void path_destroy(struct path *p)
{
	if (!p)
		return;

	free(p);
}

static void * path_run(void *arg)
{
	struct path *p = (struct path *) arg;
	struct pollfd pfd;
	struct msg m;

	pfd.fd = p->in->sd;
	pfd.events = POLLIN;

	print(DEBUG, "Established path: %12s => %s => %-12s", p->in->name, NAME, p->out->name);

	/* main thread loop */
	while (p->state == RUNNING) {
		/* wait for new incoming messages */
		//poll(&pfd, 1, 1);

		/* receive message */
		node_recv(p->in, &m);

		/* call hooks */
		for (int i = 0; i < MAX_HOOKS && p->hooks[i]; i++) {
			p->hooks[i](&m);
		}

		/* send messages */
		node_send(p->out, &m);
	}

	return NULL;
}

int path_start(struct path *p)
{
	if (!p)
		return -EFAULT;

	p->state = RUNNING;
	pthread_create(&p->tid, NULL, &path_run, (void *) p);
}

int path_stop(struct path *p)
{
	void * ret;

	if (!p)
		return -EFAULT;

	p->state = STOPPED;

	pthread_cancel(p->tid);
	pthread_join(p->tid, &ret);

	return 0; // TODO
}
