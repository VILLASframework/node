/** Websocket message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <endian.h>

#include "webmsg.h"
#include "webmsg_format.h"

void webmsg_ntoh(struct webmsg *m)
{
	webmsg_hdr_ntoh(m);

	for (int i = 0; i < m->length; i++)
		m->data[i].i = le32toh(m->data[i].i);
}

void webmsg_hton(struct webmsg *m)
{
	for (int i = 0; i < m->length; i++)
		m->data[i].i = htole32(m->data[i].i);

	webmsg_hdr_hton(m);
}

void webmsg_hdr_hton(struct webmsg *m)
{
	m->length   = htole16(m->length);
	m->sequence = htole32(m->sequence);
	m->ts.sec   = htole32(m->ts.sec);
	m->ts.nsec  = htole32(m->ts.nsec);
}

void webmsg_hdr_ntoh(struct webmsg *m)
{
	m->length   = le16toh(m->length);
	m->sequence = le32toh(m->sequence);
	m->ts.sec   = le32toh(m->ts.sec);
	m->ts.nsec  = le32toh(m->ts.nsec);
}

int webmsg_verify(struct webmsg *m)
{
	if      (m->version != WEBMSG_VERSION)
		return -1;
	else if (m->type    != WEBMSG_TYPE_DATA)
		return -2;
	else if (m->rsvd1 != 0)
		return -3;
	else
		return 0;
}