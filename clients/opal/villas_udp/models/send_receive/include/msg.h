/** Message related functions
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

/* Forward declarations. */
struct msg;

/** Swaps the byte-order of the message.
 *
 * Message are always transmitted in network (big endian) byte order.
 *
 * @param m A pointer to the message
 */
void msg_hdr_ntoh(struct msg *m);

void msg_hdr_hton(struct msg *m);

void msg_ntoh(struct msg *m);

void msg_hton(struct msg *m);

/** Check the consistency of a message.
 *
 * The functions checks the header fields of a message.
 *
 * @param m A pointer to the message
 * @retval 0 The message header is valid.
 * @retval <0 The message header is invalid.
 */
int msg_verify(struct msg *m);