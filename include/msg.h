/**
 * Message format
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _MSG_H_
#define _MSG_H_

#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "node.h"

/**
 * The structure of a message (OPAL-RT example format)
 *
 * This struct defines the format of a message.
 * Its declared as "packed" because it represents the "on wire" data.
 */

#if PROTOCOL != 0
  #error "Unknown protocol version!"
#endif

struct msg
{
	/// Sender device ID
	uint16_t dev_id;
	/// Message ID
	uint32_t msg_id;
	/// Message length (data only)
	uint16_t msg_len;
	/// Message length (data only)
	double data[MAX_VALUES];
} __attribute__((packed));

/**
 * Print a raw UDP packge in human readable form
 *
 * @param fd The file descriptor
 * @param msg A pointer to the UDP message
 */
void msg_fprint(FILE *f, struct msg *m);

/**
 * Craft message with random-walking values
 *
 * NOTE: random-walking behaviour is not reentrant!
 *
 * @param msg A pointer to a struct msg
 * @param dev_id The device id of the message
 */
void msg_random(struct msg *m, short dev_id);

#endif /* _MSG_H_ */
