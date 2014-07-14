/** Message format
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 * @file
 */

#ifndef _MSG_FORMAT_H_
#define _MSG_FORMAT_H_

#include <stdint.h>

/** Maximum number of dword values in a message */
#define MSG_VALUES	16
/** The current version number for the message format */
#define MSG_VERSION	0

/** @todo Implement more message types */
#define MSG_TYPE_DATA	0
#define MSG_TYPE_START	1
#define MSG_TYPE_STOP	2

/** This message format is used by all clients
 *
 * @diafile msg_format.dia
 **/
struct msg
{
	/** The version specifies the format of the remaining message */
	unsigned version	: 4;
	/** Data or control message */
	unsigned type		: 2;
	/** These bits are reserved for future extensions */
	unsigned __padding	: 2;
	/** Number of valid dword values in msg::data[] */
	uint8_t length;
	/** The sequence number gets incremented by one for consecutive messages */
	uint16_t sequence;
	/** The message payload */
	float data[MSG_VALUES];
} __attribute__((packed));

#endif /* _MSG_FORMAT_H_ */
