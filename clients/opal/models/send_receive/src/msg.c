/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifdef __linux__
 #include <byteswap.h>
#elif defined(__powerpc__)
 #include <xil_io.h>
#endif

#include "msg.h"

void msg_swap(struct msg *m)
{
	int i;
	for (i = 0; i < m->length; i++) {
#ifdef __linux__
		m->data[i].i = bswap_32(m->data[i].i);
#elif defined(__powerpc__)
		m->data[i].i = Xil_EndianSwap32(m->data[i].i);
#endif
	}

	m->endian ^= 1;
}

