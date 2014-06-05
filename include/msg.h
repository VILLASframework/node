/** Message format and message related functions
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file msg.h
 */

#ifndef _MSG_H_
#define _MSG_H_

#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "node.h"

#if PROTOCOL == 0
/** The format of a message (OPAL-RT example format).
 *
 * This struct defines the format of a message (protocol version 0).
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
#elif PROTOCOL == 1
/** Next generation message format for RTDS integration.
 *
 * This struct defines the format of a message (protocol version 1).
 * Its declared as "packed" because it represents the "on wire" data.
 */
struct msg
{
	struct
	{
		/// Protocol version
		unsigned version : 4;
		/// Header length
		unsigned hdr_len : 4;
		/// Message flags
		uint8_t flags;
		/// Sender device ID
		uint16_t dev_id;
		/// Message ID
		uint32_t sequence;
		/// Message length (data only)
		uint16_t data_len;
		/// Digital signature for authentication
		uint32_t signature;
		/// Timestamp in uS since unix epoch
		uint64_t timestamp
	} header;
	union
	{
		uint32_t integer;
		float data float_single;
		char * data_str;
	} data[MAX_VALUES];

} __attribute__((packed));
#else
  #error "Unknown protocol version!"
#endif

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
