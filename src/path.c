/**
 * Message paths
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <string.h>
#include <stdlib.h>
#include <poll.h>

#include "utils.h"
#include "path.h"

struct path* path_create(struct node *in, struct node *out[], int len)
{
	struct path *p = malloc(sizeof(struct path));
	if (!p)
		return NULL;

	p->in = in;

	for (int i = 0; i < len; i++) {
		p->out[i] = out[i];
	}

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

	// TODO: add support for multiple outgoing nodes
	print(DEBUG, "Established path: %12s => %s => %-12s", p->in->name, NAME, p->out[0]->name);

	/* main thread loop */
	while (p->state == RUNNING) {
		/* wait for new incoming messages */
		//poll(&pfd, 1, 1);

		/* receive message */
		node_recv(p->in, &m);

		/* call hooks */

		/* send messages */
		/*for (struct node **n = p->out; *n; n++) {
			node_send(*n, &m);
		}*/
	}

	return NULL;
}

int path_start(struct path *p)
{
	p->state = RUNNING;
	pthread_create(&p->tid, NULL, &path_run, (void *) p);
}

int path_stop(struct path *p)
{
	void * ret;

	p->state = STOPPED;

	pthread_cancel(p->tid);
	pthread_join(p->tid, &ret);

	return 0; // TODO
}
