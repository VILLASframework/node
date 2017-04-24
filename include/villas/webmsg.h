/** Message related functions
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#pragma once

/* Forward declarations. */
struct webmsg;

/** Swaps the byte-order of the message.
 *
 * Message are always transmitted in network (big endian) byte order.
 *
 * @param m A pointer to the message
 */
void webmsg_hdr_ntoh(struct webmsg *m);

void webmsg_hdr_hton(struct webmsg *m);

void webmsg_ntoh(struct webmsg *m);

void webmsg_hton(struct webmsg *m);

/** Check the consistency of a message.
 *
 * The functions checks the header fields of a message.
 *
 * @param m A pointer to the message
 * @retval 0 The message header is valid.
 * @retval <0 The message header is invalid.
 */
int msg_verify(struct webmsg *m);