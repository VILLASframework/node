/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/

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

int msg_verify(struct msg *m)
{
	if      (m->version != MSG_VERSION)
		return -1;
	else if (m->type    != MSG_TYPE_DATA)
		return -2;
	else if ((m->length <= 0) || (m->length > MSG_VALUES))
		return -3;
	else if ((m->rsvd1 != 0)  || (m->rsvd2 != 0))
		return -4;
	else
		return 0;
}