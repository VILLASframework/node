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

/** Print a raw UDP message in human readable form.
 *
 * @param f The file stream
 * @param msg A pointer to the message
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int msg_fprint(FILE *f, struct msg *m);

/** Read a message from a file stream.
 *
 * @param f The file stream
 * @param m A pointer to the message
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int msg_fscan(FILE *f, struct msg *m);

/** Change the values of an existing message in a random fashion.
 *
 * @param m A pointer to the message whose values will be updated
 */
void msg_random(struct msg *m);

/** Send a message to a node.
 *
 * @param m A pointer to the message
 * @param n A pointer to the node
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int msg_send(struct msg *m, struct node *n);

/** Receive a message from a node.
 *
 * @param m A pointer to the message
 * @param n A pointer to the node
 * @return
 *  - 0 on success
 *  - otherwise an error occured
 */
int msg_recv(struct msg *m, struct node *n);

#endif /* _MSG_H_ */
