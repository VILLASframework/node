/** Message format
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _MSG_FORMAT_H_
#define _MSG_FORMAT_H_

#include <stdint.h>

#ifndef BYTE_ORDER
 #define _BSD_SOURCE    	1
 #include <endian.h>
#endif

/** Maximum number of dword values in a message */
#define MSG_VALUES		16

/** The current version number for the message format */
#define MSG_VERSION		0

/** @todo Implement more message types */
#define MSG_TYPE_DATA		0 /**< Message contains float values */
#define MSG_TYPE_START		1 /**< Message marks the beginning of a new simulation case */
#define MSG_TYPE_STOP		2 /**< Message marks the end of a simulation case */

#define MSG_ENDIAN_LITTLE       0 /**< Message values are in little endian format (float too!) */
#define MSG_ENDIAN_BIG          1 /**< Message values are in bit endian format */

#if BYTE_ORDER == LITTLE_ENDIAN
 #define MSG_ENDIAN_HOST        MSG_ENDIAN_LITTLE
#elif BYTE_ORDER == BIG_ENDIAN
 #define MSG_ENDIAN_HOST        MSG_ENDIAN_BIG
#endif

/** The total length of a message */
#define MSG_LEN(values)		(4 * (values + 1))

/** Initialize a message */
#define MSG_INIT(i)	{ \
	.version = MSG_VERSION, \
	.type = MSG_TYPE_DATA, \
	.endian = MSG_ENDIAN_HOST, \
	.length = i, \
	.sequence = 0 \
}

/** This message format is used by all clients
 *
 * @diafile msg_format.dia
 **/
struct msg
{
	unsigned version: 4; /**< Specifies the format of the remaining message (see MGS_VERSION) */
	unsigned type	: 2; /**< Data or control message (see MSG_TYPE_*) */
	unsigned endian	: 1; /**< Specifies the byteorder of the message (see MSG_ENDIAN_*) */
	unsigned 	: 1; /**< Reserved padding bits */

	/** Number of valid dword values in msg::data[] */
	uint8_t length;
	/** The sequence number gets incremented by one for consecutive messages */
	uint16_t sequence;
	/** The message payload */
	union {
		float f;
		uint32_t i;
	} data[MSG_VALUES];
} __attribute__((packed));

#endif /* _MSG_FORMAT_H_ */
