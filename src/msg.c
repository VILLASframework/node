/**
 * Message related functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "msg.h"

void msg_fprint(FILE *f, struct msg *msg)
{
	fprintf(f, "p: dev_id = %u, msg_id = %u, data", msg->dev_id, msg->msg_id);

	for (int i = 0; i < msg->msg_len / sizeof(double); i++)
		fprintf(f, "\t%f", msg->data[i]);

	fprintf(f, "\n");
}

void msg_random(struct msg *msg, short dev_id)
{
	static uint16_t msg_id;
	static double data[4];

	for (int i = 0; i < MAX_VALUES; i++)
		data[i] += (double) random() / RAND_MAX - .5;

	msg->msg_id = ++msg_id;
	msg->dev_id = dev_id;
	msg->msg_len = sizeof(data);
	memcpy(&msg->data, data, msg->msg_len);
}

