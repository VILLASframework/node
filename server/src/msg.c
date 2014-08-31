/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <byteswap.h>
#include <arpa/inet.h>

#include "msg.h"
#include "node.h"
#include "utils.h"

void msg_swap(struct msg *m)
{
	uint32_t *data = (uint32_t *) m->data;


	/* Swap data */
	for (int i = 0; i < m->length; i++)
		data[i] = bswap_32(data[i]);

	m->endian ^= 1;
}

int msg_fprint(FILE *f, struct msg *m)
{
	assert(m->endian == MSG_ENDIAN_HOST);

	fprintf(f, "%-8hu", m->sequence);

	for (int i = 0; i < m->length; i++)
		fprintf(f, "%-12.6f ", m->data[i].f);

	fprintf(f, "\n");

	return 0;
}

int msg_fscan(FILE *f, struct msg *m)
{
	fscanf(f, "%8hu ", &m->sequence);

	for (int i = 0; i < m->length; i++)
		fscanf(f, "%12f ", &m->data[i].f);

	fscanf(f, "\n");

	m->endian = MSG_ENDIAN_HOST;

	return 0;
}

void msg_random(struct msg *m)
{
	for (int i = 0; i < m->length; i++)
		m->data[i].f += (float) random() / RAND_MAX - .5;

	m->endian = MSG_ENDIAN_HOST;
	m->sequence++;
}

int msg_send(struct msg *m, struct node *n)
{
	/* Convert headers to network byte order */
	m->sequence = ntohs(m->sequence);

	if (sendto(n->sd, m, MSG_LEN(m->length), 0,
	    (struct sockaddr *) &n->remote,
	    sizeof(struct sockaddr_in)) < 0)
		perror("Failed sendto");

	debug(10, "Message sent to node '%s'", n->name);

	return 0;
}

int msg_recv(struct msg *m, struct node *n)
{
	/** @todo Fix this for multiple paths calling msg_recv. */

	/* Receive message from socket */
	if (recv(n->sd, m, sizeof(struct msg), 0) < 0)
		perror("Failed recv");

	/* Convert headers to host byte order */
	m->sequence = htons(m->sequence);

	/* Convert message to host endianess */
	if (m->endian != MSG_ENDIAN_HOST)
		msg_swap(m);

	debug(10, "Message received from node '%s'", n->name);

	return 0;
}
