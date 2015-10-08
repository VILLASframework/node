/** Message related functions
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _MSG_H_
#define _MSG_H_

#include <stdio.h>

#include "msg_format.h"

struct node;

/** These flags define the format which is used by msg_fscan() and msg_fprint(). */
enum msg_flags {
	MSG_PRINT_NANOSECONDS	= 1,
	MSG_PRINT_OFFSET	= 2,
	MSG_PRINT_SEQUENCE	= 4,
	MSG_PRINT_VALUES	= 8,

	MSG_PRINT_ALL		= 0xFF
};

/** Swaps message contents byte-order.
 *
 * Message can either be transmitted in little or big endian
 * format. The actual endianess for a message is defined by the
 * msg::endian field. This covers msg::length, msg::sequence, msg::data and msg::ts fields.
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

/** Print a raw message in human readable form to a file stream.
 *
 * @param f The file handle from fopen() or stdout, stderr.
 * @param m A pointer to the message.
 * @param flags See msg_flags.
 * @param offset A optional offset in seconds. Only used if flags contains MSG_PRINT_OFFSET.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int msg_fprint(FILE *f, struct msg *m, int flags, double offset);

/** Read a message from a file stream.
 *
 * @param f The file handle from fopen() or stdin.
 * @param m A pointer to the message.
 * @param flags et MSG_PRINT_* flags for each component found in sample if not NULL. See msg_flags.
 * @param offset Write offset to this pointer if not NULL.
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int msg_fscan(FILE *f, struct msg *m, int *flags, double *offset);

/** Change the values of an existing message in a random fashion.
 *
 * @param m A pointer to the message whose values will be updated
 */
void msg_random(struct msg *m);

#endif /* _MSG_H_ */
