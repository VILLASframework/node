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

#if PROTOCOL == 0
/**
 * The format of a message (OPAL-RT example format)
 *
 * This struct defines the format of a message (protocol version 0)
 * Its declared as "packed" because it represents the "on wire" data.
 */

struct msg
{
	/// Sender device ID
	uint16_t device;
	/// Message ID
	uint32_t sequence;
	/// Message length (data only)
	uint16_t length;
	/// Message data
	double data[MAX_VALUES];
} __attribute__((packed));
/**
 * @brief Print a raw UDP packge in human readable form
 *
 * @param f The file stream
 * @param msg A pointer to the message
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int msg_fprint(FILE *f, struct msg *m);

/**
 * @brief Read a message from a file stream
 *
 * @param f The file stream
 * @param m A pointer to the message
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int msg_fscan(FILE *f, struct msg *m);

/**
 * @brief Change the values of an existing message in a random fashion
 *
 * @param msg A pointer to the message whose values will be updated
 */
void msg_random(struct msg *m);

/**
 *
 *
 */

#endif /* _MSG_H_ */
