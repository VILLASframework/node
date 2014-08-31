/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <byteswap.h>

#include "msg.h"

void msg_swap(struct msg *m)
{
	uint32_t *data = (uint32_t *) m->data;


	/* Swap data */
	for (int i = 0; i < m->length; i++)
		data[i] = bswap_32(data[i]);

	m->endian ^= 1;
}

