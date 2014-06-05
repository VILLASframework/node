/**
 * Message related functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <string.h>

#include "msg.h"

int msg_fprint(FILE *f, struct msg *msg)
{
	fprintf(f, "%-8u %-8u", msg->device, msg->sequence);

	for (int i = 0; i < msg->length / sizeof(double); i++) {
		fprintf(f, "%-12.6f ", msg->data[i]);
	}

	fprintf(f, "\n");

	return 0;
}

int msg_fscan(FILE *f, struct msg *msg)
{
	fscanf(f, "%8u %8u ", &msg->device, &msg->sequence);

	for (int i = 0; i < msg->length / sizeof(double); i++) {
		fscanf(f, "%12lf ", &msg->data[i]);
	}

	fscanf(f, "\n");

	return 0;
}

void msg_random(struct msg *m)
{
	int values = m->length / sizeof(double);

	for (int i = 0; i < values; i++) {
		m->data[i] += (double) random() / RAND_MAX - .5;
	}

	m->sequence++;
}

