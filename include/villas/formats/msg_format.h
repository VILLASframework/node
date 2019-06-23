/** Message format
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#pragma once

#include <cstdint>

/** The current version number for the message format */
#define MSG_VERSION		2

/** @todo Implement more message types */
#define MSG_TYPE_DATA		0 /**< Message contains float / integer values */
#define MSG_TYPE_START		1 /**< Message marks the beginning of a new simulation case */
#define MSG_TYPE_STOP		2 /**< Message marks the end of a simulation case */

/** The total size in bytes of a message */
#define MSG_LEN(values)		(sizeof(struct msg) + MSG_DATA_LEN(values))

/** The length of \p values values in bytes. */
#define MSG_DATA_LEN(values)	(sizeof(float) * (values))

/** The offset to the first data value in a message. */
#define MSG_DATA_OFFSET(msg)	((char *) (msg) + offsetof(struct msg, data))

/** The timestamp of a message in struct timespec format */
#define MSG_TS(msg, i)  \
	i.tv_sec  = (msg)->ts.sec;	\
	i.tv_nsec = (msg)->ts.nsec;

/** This message format is used by all clients
 *
 * @diafile msg_format.dia
 **/
struct msg
{
#if BYTE_ORDER == BIG_ENDIAN
	unsigned version: 4;	/**< Specifies the format of the remaining message (see MGS_VERSION) */
	unsigned type	: 2;	/**< Data or control message (see MSG_TYPE_*) */
	unsigned rsvd1	: 2;	/**< Reserved bits */
#elif BYTE_ORDER == LITTLE_ENDIAN
	unsigned rsvd1	: 2;	/**< Reserved bits */
	unsigned type	: 2;	/**< Data or control message (see MSG_TYPE_*) */
	unsigned version: 4;	/**< Specifies the format of the remaining message (see MGS_VERSION) */
#else
  #error Invalid byte-order
#endif

	uint8_t resv2;		/**< An id which identifies the source of this sample. */
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
