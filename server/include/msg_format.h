/** Message format
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _MSG_FORMAT_H_
#define _MSG_FORMAT_H_

#include <stdint.h>

#ifdef __linux__
 #define _BSD_SOURCE    	1
 #include <endian.h>
#elif defined(__PPC__) /* Xilinx toolchain */
 #include <lwip/arch.h>
#endif

#include "config.h"

/** Maximum number of dword values in a message */
#define MSG_VALUES		MAX_VALUES

/** The current version number for the message format */
#define MSG_VERSION		0

/** @todo Implement more message types */
#define MSG_TYPE_DATA		0 /**< Message contains float values */
#define MSG_TYPE_START		1 /**< Message marks the beginning of a new simulation case */
#define MSG_TYPE_STOP		2 /**< Message marks the end of a simulation case */

#define MSG_ENDIAN_LITTLE	0 /**< Message values are in little endian format (float too!) */
#define MSG_ENDIAN_BIG		1 /**< Message values are in bit endian format */

#if BYTE_ORDER == LITTLE_ENDIAN
 #define MSG_ENDIAN_HOST        MSG_ENDIAN_LITTLE
#elif BYTE_ORDER == BIG_ENDIAN
 #define MSG_ENDIAN_HOST        MSG_ENDIAN_BIG
#else
 #error "Unknown byte order!"
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
#if BYTE_ORDER == BIG_ENDIAN
	unsigned version: 4; /**< Specifies the format of the remaining message (see MGS_VERSION) */
	unsigned type	: 2; /**< Data or control message (see MSG_TYPE_*) */
	unsigned endian	: 1; /**< Specifies the byteorder of the message (see MSG_ENDIAN_*) */
	unsigned	: 1; /**< Reserved padding bits */
#elif BYTE_ORDER == LITTLE_ENDIAN
	unsigned	: 1; /**< Reserved padding bits */
	unsigned endian	: 1; /**< Specifies the byteorder of the message (see MSG_ENDIAN_*) */
	unsigned type	: 2; /**< Data or control message (see MSG_TYPE_*) */
	unsigned version: 4; /**< Specifies the format of the remaining message (see MGS_VERSION) */
#endif

	/** Number of valid dword values in msg::data[] */
	uint8_t length;
	/** The sequence number gets incremented by one for consecutive messages */
	uint16_t sequence;

	/** The message payload */
	union {
		float f;	/**< Floating point values (note msg::endian) */
		uint32_t i;	/**< Integer values (note msg::endian) */
	} data[MSG_VALUES];
} __attribute__((aligned(4), packed));

#endif /* _MSG_FORMAT_H_ */
