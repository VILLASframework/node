/** Message format
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

#ifndef _MSG_FORMAT_H_
#define _MSG_FORMAT_H_

#include <stdint.h>

#ifdef __linux__
  #define _BSD_SOURCE    	1
  #include <endian.h>
#elif defined(__PPC__) /* Xilinx toolchain */
  #include <lwip/arch.h>
#endif

/** The current version number for the message format */
#define MSG_VERSION		1

/** @todo Implement more message types */
#define MSG_TYPE_DATA		0 /**< Message contains float values */
#define MSG_TYPE_START		1 /**< Message marks the beginning of a new simulation case */
#define MSG_TYPE_STOP		2 /**< Message marks the end of a simulation case */

#define MSG_ENDIAN_LITTLE	0 /**< Message values are in little endian format (float too!) */
#define MSG_ENDIAN_BIG		1 /**< Message values are in bit endian format */

#if BYTE_ORDER == LITTLE_ENDIAN
  #define MSG_ENDIAN_HOST	MSG_ENDIAN_LITTLE
#elif BYTE_ORDER == BIG_ENDIAN
  #define MSG_ENDIAN_HOST	MSG_ENDIAN_BIG
#else
  #error "Unknown byte order!"
#endif

/** The total size in bytes of a message */
#define MSG_LEN(values)		(sizeof(struct msg) + MSG_DATA_LEN(values))

/** The length of \p values values in bytes. */
#define MSG_DATA_LEN(values)	(sizeof(float) * (values))

/** The offset to the first data value in a message. */
#define MSG_DATA_OFFSET(msg)	((char *) (msg) + offsetof(struct msg, data))

/** Initialize a message with default values */
#define MSG_INIT(val, seq) (struct msg) {\
	.version = MSG_VERSION,		\
	.type = MSG_TYPE_DATA,		\
	.endian = MSG_ENDIAN_HOST,	\
	.values = val,	 		\
	.sequence = seq			\
}

/** The timestamp of a message in struct timespec format */
#define MSG_TS(msg) (struct timespec) {	\
	.tv_sec  = (msg)->ts.sec,	\
	.tv_nsec = (msg)->ts.nsec	\
}

/** This message format is used by all clients
 *
 * @diafile msg_format.dia
 **/
struct msg
{
#if BYTE_ORDER == BIG_ENDIAN
	unsigned version: 4;	/**< Specifies the format of the remaining message (see MGS_VERSION) */
	unsigned type	: 2;	/**< Data or control message (see MSG_TYPE_*) */
	unsigned endian	: 1;	/**< Specifies the byteorder of the message (see MSG_ENDIAN_*) */
	unsigned rsvd1	: 1;	/**< Reserved bits */
#elif BYTE_ORDER == LITTLE_ENDIAN
	unsigned rsvd1	: 1;	/**< Reserved bits */
	unsigned endian	: 1;	/**< Specifies the byteorder of the message (see MSG_ENDIAN_*) */
	unsigned type	: 2;	/**< Data or control message (see MSG_TYPE_*) */
	unsigned version: 4;	/**< Specifies the format of the remaining message (see MGS_VERSION) */
#endif
	unsigned rsvd2	: 8;	/**< Reserved bits */
	
	uint16_t values;	/**< The number of values in msg::data[]. Endianess is specified in msg::endian. */
	uint32_t sequence;	/**< The sequence number is incremented by one for consecutive messages. Endianess is specified in msg::endian. */
	
	/** A timestamp per message. Endianess is specified in msg::endian. */
	struct {
		uint32_t sec;	/**< Seconds since 1970-01-01 00:00:00 */
		uint32_t nsec;	/**< Nanoseconds of the current second. */
	} ts;

	/** The message payload. Endianess is specified in msg::endian. */
	union {
		float    f;	/**< Floating point values (note msg::endian) */
		uint32_t i;	/**< Integer values (note msg::endian) */
	} data[];
} __attribute__((packed));

#endif /* _MSG_FORMAT_H_ */
