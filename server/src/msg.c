/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifdef __linux__
 #include <byteswap.h>
#elif defined(__PPC__) /* Xilinx toolchain */
 #include <xil_io.h>
 #define bswap_32(x)	Xil_EndianSwap32(x)
#endif

#include "msg.h"
#include "node.h"
#include "utils.h"

void msg_swap(struct msg *m)
{
	int i;
	for (i = 0; i < m->length; i++)
		m->data[i].i = bswap_32(m->data[i].i);

	m->endian ^= 1;
}

int msg_fprint(FILE *f, struct msg *m)
{
	if (m->endian != MSG_ENDIAN_HOST)
		msg_swap(m);

	fprintf(f, "%hu", m->sequence);

	for (int i = 0; i < m->length; i++)
		fprintf(f, "\t%.6f", m->data[i].f);

	fprintf(f, "\n");

	return 0;
}

int msg_fscan(FILE *f, struct msg *m)
{
	char line[MSG_VALUES * 16];
	char *ptr = line;

	if (!fgets(line, sizeof(line), f))
		return 0;

	m->sequence = (uint16_t) strtol(ptr, &ptr, 10);

	int i;
	for (i = 0; i <= MSG_VALUES; i++) {
		while(isblank(*ptr++));
		if (*ptr == '\n' || *ptr == '\0')
			break;

		m->data[i].f = strtod(ptr, &ptr);
	}

	m->length = i;
	m->endian = MSG_ENDIAN_HOST;
	
	return m->length;
}

void msg_random(struct msg *m)
{
	for (int i = 0; i < m->length; i++)
		m->data[i].f += (float) random() / RAND_MAX - .5;

	m->endian = MSG_ENDIAN_HOST;
}

int msg_send(struct msg *m, struct node *n)
{
	/* Convert headers to network byte order */
	m->sequence = htons(m->sequence);

	if (sendto(n->sd, m, MSG_LEN(m->length), 0,
	    (struct sockaddr *) &n->remote,
	    sizeof(struct sockaddr_in)) < 0)
		perror("Failed sendto");

	debug(10, "Message sent to node '%s': version=%u, type=%u, endian=%u, length=%u, sequence=%u",
		n->name, m->version, m->type, m->endian, m->length, ntohs(m->sequence));

	return 0;
}

int msg_recv(struct msg *m, struct node *n)
{
	/** @todo Fix this for multiple paths calling msg_recv. */

	/* Receive message from socket */
	if (recv(n->sd, m, sizeof(struct msg), 0) < 0) {
		if (errno == EINTR)
			return -EINTR;

		perror("Failed recv");
	}

	/* Convert headers to host byte order */
	m->sequence = ntohs(m->sequence);

	/* Convert message to host endianess */
	if (m->endian != MSG_ENDIAN_HOST)
		msg_swap(m);

	debug(10, "Message received from node '%s': version=%u, type=%u, endian=%u, length=%u, sequence=%u",
		n->name, m->version, m->type, m->endian, m->length, m->sequence);

	return 0;
}
