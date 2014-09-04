/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifdef __linux__
 #include <byteswap.h>
#elif defined(__PPC__) /* Xilinx toolchain */
 #include <xil_io.h>
 #define bswap_32(x)	Xil_EndianSwap32(x)
#endif

#include "msg.h"

void msg_swap(struct msg *m)
{
	int i;
	for (i = 0; i < m->length; i++)
		m->data[i].i = bswap_32(m->data[i].i);

	m->endian ^= 1;
}

