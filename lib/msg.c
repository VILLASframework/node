/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
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
  #define bswap_16(x)	Xil_EndianSwap16(x)
  #define bswap_32(x)	Xil_EndianSwap32(x)
#endif

#include "msg.h"
#include "node.h"
#include "utils.h"

struct msg * msg_create(size_t values) {
	struct msg m = {
		.version = MSG_VERSION,
		.type = MSG_TYPE_DATA,
		.endian = MSG_ENDIAN_HOST,
		.values = values,
		.sequence = 0,
		.rsvd1 = 0, .rsvd2 = 0
	};
	
	return memdup(&m, sizeof(struct msg) + sizeof(float) * values);
}

void msg_destroy(struct msg *m)
{
	free(m);
}

void msg_swap(struct msg *m)
{
	m->values   = bswap_16(m->values);
	m->sequence = bswap_32(m->sequence);
	m->ts.sec   = bswap_32(m->ts.sec);
	m->ts.nsec  = bswap_32(m->ts.nsec);
	
	for (int i = 0; i < m->values; i++)
		m->data[i].i = bswap_32(m->data[i].i);

	m->endian ^= 1;
}

int msg_verify(struct msg *m)
{
	if      (m->version != MSG_VERSION)
		return -1;
	else if (m->type    != MSG_TYPE_DATA)
		return -2;
	else if ((m->rsvd1 != 0)  || (m->rsvd2 != 0))
		return -3;
	else
		return 0;
}

int msg_print(char *buf, size_t len, struct msg *m, int flags, double offset)
{
	size_t off = snprintf(buf, len, "%u", m->ts.sec);
	
	if (flags & MSG_PRINT_NANOSECONDS)
		off += snprintf(buf + off, len - off, ".%09u", m->ts.nsec);
	
	if (flags & MSG_PRINT_OFFSET)
		off += snprintf(buf + off, len - off, "%+g", offset);
	
	if (flags & MSG_PRINT_SEQUENCE)
		off += snprintf(buf + off, len - off, "(%u)", m->sequence);

	if (flags & MSG_PRINT_VALUES) {
		for (int i = 0; i < m->values; i++)
			off += snprintf(buf + off, len - off, "\t%.6f", m->data[i].f);
	}

	off += snprintf(buf + off, len - off, "\n");

	return off + 1; /* trailing '\0' */
}

int msg_scan(const char *line, struct msg *m, int *fl, double *off)
{
	char *end;
	const char *ptr = line;

	int flags = 0;
	double offset;
	
	m->version = MSG_VERSION;
	m->endian  = MSG_ENDIAN_HOST;
	m->rsvd1   = m->rsvd2   = 0;
	
	/* Format: Seconds.NanoSeconds+Offset(SequenceNumber) Value1 Value2 ... 
	 * RegEx: (\d+(?:\.\d+)?)([-+]\d+(?:\.\d+)?(?:e[+-]?\d+)?)?(?:\((\d+)\))?
	 *
	 * Please note that only the seconds and at least one value are mandatory
	 */

	/* Mandatory: seconds */
	m->ts.sec = (uint32_t) strtoul(ptr, &end, 10);
	if (ptr == end)
		return -2;

	/* Optional: nano seconds */
	if (*end == '.') {
		ptr = end + 1;

		m->ts.nsec = (uint32_t) strtoul(ptr, &end, 10);
		if (ptr != end)
			flags |= MSG_PRINT_NANOSECONDS;
		else
			return -3;
	}
	else
		m->ts.nsec = 0;

	/* Optional: offset / delay */
	if (*end == '+' || *end == '-') {
		ptr = end + 1;

		offset = strtof(ptr, &end); /* offset is ignored for now */
		if (ptr != end)
			flags |= MSG_PRINT_OFFSET;
		else
			return -4;
	}

	/* Optional: sequence number */
	if (*end == '(') {
		ptr = end + 1;

		m->sequence = (uint16_t) strtoul(ptr, &end, 10);
		if (ptr != end && *end == ')')
			flags |= MSG_PRINT_SEQUENCE;
		else
			return -5;
		
		end = end + 1;
	}
	else
		m->sequence = 0;

	for (m->values = 0, ptr  = end; ;
	     m->values++,   ptr = end) {

		/** @todo We only support floating point values at the moment */
		m->data[m->values].f = strtod(ptr, &end);

		if (end == ptr) /* there are no valid FP values anymore */
			break;
	}
	
	if (m->values > 0)
		flags |= MSG_PRINT_VALUES;
	
	if (fl)
		*fl = flags;
	if (off && (flags & MSG_PRINT_OFFSET))
		*off = offset;

	return m->values;
}

int msg_fprint(FILE *f, struct msg *m, int flags, double offset)
{
	char line[4096];

	int len = msg_print(line, sizeof(line), m, flags, offset);
	
	fputs(line, f);
	
	return len;
}

int msg_fscan(FILE *f, struct msg *m, int *fl, double *off)
{
	char *ptr, line[4096];
	
skip:	if (fgets(line, sizeof(line), f) == NULL)
		return -1; /* An error occured */

	/* Skip whitespaces, empty and comment lines */
	for (ptr = line; isspace(*ptr); ptr++);
	if (*ptr == '\0' || *ptr == '#')
		goto skip;
	
	return msg_scan(line, m, fl, off);
}
