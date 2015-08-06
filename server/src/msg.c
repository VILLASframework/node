/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __linux__
 #include <byteswap.h>
#elif defined(__PPC__) /* Xilinx toolchain */
 #include <xil_io.h>
 #define bswap_32(x)	Xil_EndianSwap32(x)
#endif

#include "msg.h"
#include "node.h"
#include "utils.h"

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

int msg_fprint(FILE *f, struct msg *m)
{
	if (m->endian != MSG_ENDIAN_HOST)
		msg_swap(m);

	fprintf(f, "%10u.%09u\t%hu", m->ts.sec, m->ts.nsec, m->sequence);

	for (int i = 0; i < m->length; i++)
		fprintf(f, "\t%.6f", m->data[i].f);

	fprintf(f, "\n");

	return 0;
}

/** @todo Currently only floating point values are supported */
int msg_fscan(FILE *f, struct msg *m)
{
	char line[MSG_VALUES * 16];
	char *next, *ptr = line;

	if (!fgets(line, sizeof(line), f))
		return 0;

	m->ts.sec   = (uint32_t) strtoul(ptr, &ptr, 10); ptr++;
	m->ts.nsec  = (uint32_t) strtoul(ptr, &ptr, 10);
	m->sequence = (uint16_t) strtoul(ptr, &ptr, 10);

	m->version = MSG_VERSION;
	m->endian  = MSG_ENDIAN_HOST;
	m->length  = 0;
	m->rsvd1   = 0;
	m->rsvd2   = 0;

	while (m->length < MSG_VALUES) {
		m->data[m->length].f = strtod(ptr, &next);

		if (next == ptr)
			break;

		ptr = next;
		m->length++;
	}

	return m->length;
}

void msg_random(struct msg *m)
{
	for (int i = 0; i < m->length; i++)
		m->data[i].f += box_muller(0, 1);

	m->endian = MSG_ENDIAN_HOST;
}
