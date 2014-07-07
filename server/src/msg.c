/** Message related functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include "msg.h"
#include "node.h"
#include "utils.h"

int msg_fprint(FILE *f, struct msg *msg)
{
	fprintf(f, "%-8hu", msg->sequence);

	for (int i = 0; i < msg->length; i++)
		fprintf(f, "%-12.6f ", msg->data[i]);

	fprintf(f, "\n");

	return 0;
}

int msg_fscan(FILE *f, struct msg *m)
{
	fscanf(f, "%8hu ", &m->sequence);

	for (int i = 0; i < m->length; i++)
		fscanf(f, "%12f ", &m->data[i]);

	fscanf(f, "\n");
	return 0;
}

void msg_random(struct msg *m)
{
	for (int i = 0; i < m->length; i++)
		m->data[i] += (float) random() / RAND_MAX - .5;

	m->sequence++;
}

int msg_send(struct msg *m, struct node *n)
{
	if (sendto(n->sd, m, (m->length+1) * 4, 0,
		(struct sockaddr *) &n->remote,
		sizeof(struct sockaddr_in)) < 0)
		perror("Failed sendto");

	debug(10, "Message sent to node '%s'", n->name);
	if (V >= 10) msg_fprint(stdout, m);

	return 0;
}

int msg_recv(struct msg *m, struct node *n)
{
	if (recv(n->sd, m, sizeof(struct msg), 0) < 0)
		perror("Failed recv");

	debug(10, "Message received from node '%s'", n->name);
	if (V >= 10) msg_fprint(stdout, m);

	return 0;
}
