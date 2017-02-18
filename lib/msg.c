/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#ifdef __linux__
  #include <byteswap.h>
#elif defined(__PPC__) /* Xilinx toolchain */
  #include <xil_io.h>
  #define bswap_16(x)	Xil_EndianSwap16(x)
  #define bswap_32(x)	Xil_EndianSwap32(x)
#endif

#include "msg.h"
#include "node.h"
#include "utils.h"

void msg_hdr_swap(struct msg *m)
{
	m->length   = bswap_16(m->length);
	m->sequence = bswap_32(m->sequence);
	m->ts.sec   = bswap_32(m->ts.sec);
	m->ts.nsec  = bswap_32(m->ts.nsec);

	m->endian ^= 1;
}

int msg_verify(struct msg *m)
{
	if      (m->version != MSG_VERSION)
		return -1;
	else if (m->type    != MSG_TYPE_DATA)
		return -2;
	else if ((m->rsvd1 != 0) || (m->rsvd2 != 0))
		return -3;
	else
		return 0;
}
