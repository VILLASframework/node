/** Binary websocket message format.
 *
 * Note: Messages sent by the 'websocket' node-type are always send in little endian byte-order!
 *       This is different from the messages send with the 'socket' node-type!
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#pragma once

#include <stdint.h>

/** The current version number for the message format */
#define WEBMSG_VERSION		2

/** @todo Implement more message types */
#define WEBMSG_TYPE_DATA	0 /**< Message contains float values */
#define WEBMSG_TYPE_START	1 /**< Message marks the beginning of a new simulation case */
#define WEBMSG_TYPE_STOP	2 /**< Message marks the end of a simulation case */

/** The total size in bytes of a message */
#define WEBMSG_LEN(values)	(sizeof(struct webmsg) + WEBMSG_DATA_LEN(values))

/** The length of \p values values in bytes. */
#define WEBMSG_DATA_LEN(values)	(sizeof(float) * (values))

/** The offset to the first data value in a message. */
#define WEBMSG_DATA_OFFSET(msg)	((char *) (msg) + offsetof(struct webmsg, data))

/** Initialize a message with default values */
#define WEBMSG_INIT(len, seq) (struct webmsg) {\
	.version  = WEBMSG_VERSION,	\
	.type     = WEBMSG_TYPE_DATA,	\
	.length   = len,	 	\
	.sequence = seq			\
}

/** The timestamp of a message in struct timespec format */
#define WEBMSG_TS(msg) (struct timespec) {\
	.tv_sec  = (msg)->ts.sec,	\
	.tv_nsec = (msg)->ts.nsec	\
}

/** This message format is used by all clients
 *
 * @diafile msg_format.dia
 **/
struct webmsg
{
	unsigned rsvd1	: 2;	/**< Reserved bits */
	unsigned type	: 2;	/**< Data or control message (see MSG_TYPE_*) */
	unsigned version: 4;	/**< Specifies the format of the remaining message (see MGS_VERSION) */

	uint8_t id;		/**< The node index from / to which this sample received / sent to.
				 *   Corresponds to the index of the node in the http://localhost/nodes.json  array. */
	
	uint16_t length;	/**< The number of values in msg::data[]. */
	uint32_t sequence;	/**< The sequence number is incremented by one for consecutive messages. */
	
	/** A timestamp per message. */
	struct {
		uint32_t sec;	/**< Seconds since 1970-01-01 00:00:00 */
		uint32_t nsec;	/**< Nanoseconds of the current second. */
	} ts;

	/** The message payload. */
	union {
		float    f;	/**< Floating point values. */
		uint32_t i;	/**< Integer values. */
	} data[];
} __attribute__((packed));