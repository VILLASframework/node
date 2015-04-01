/** Message related functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _MSG_H_
#define _MSG_H_

#include <stdio.h>

#include "msg_format.h"

struct node;

/** Swap a message to host byte order.
 *
 * Message can either be transmitted in little or big endian
 * format. The actual endianess for a message is defined by the
 * msg::byteorder field.
 * Received message are usally converted to the endianess of the host.
 * This is required for further sanity checks of the sequence number
 * or parsing of the data.
 *
 * @param m A pointer to the message
 */
void msg_swap(struct msg *m);

/** Check the consistency of a message.
 *
 * The functions checks the header fields of a message.
 *
 * @param m A pointer to the message
 * @retval 0 The message header is valid.
 * @retval <0 The message header is invalid.
 */
int msg_verify(struct msg *m);

/** Print a raw UDP message in human readable form.
 *
 * @param f The file stream
 * @param m A pointer to the message
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int msg_fprint(FILE *f, struct msg *m);

/** Read a message from a file stream.
 *
 * @param f The file stream
 * @param m A pointer to the message
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int msg_fscan(FILE *f, struct msg *m);

/** Change the values of an existing message in a random fashion.
 *
 * @param m A pointer to the message whose values will be updated
 */
void msg_random(struct msg *m);

#endif /* _MSG_H_ */
