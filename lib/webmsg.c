/** Websocket message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 *********************************************************************************/

#include <endian.h>

#include "webmsg.h"
#include "msg_format.h"

void webmsg_ntoh(struct webmsg *m)
{
	msg_hdr_ntoh(m);

	for (int i = 0; i < m->length; i++)
		m->data[i].i = ntohl(m->data[i].i);
}

void msg_hton(struct webmsg *m)
{
	for (int i = 0; i < m->length; i++)
		m->data[i].i = htonl(m->data[i].i);

	webmsg_hdr_hton(m);
}

void webmsg_hdr_hton(struct webmsg *m)
{
	m->length   = htole16(m->length);
	m->sequence = htonle32(m->sequence);
	m->ts.sec   = htonle32(m->ts.sec);
	m->ts.nsec  = htonle32(m->ts.nsec);
}

void webmsg_hdr_ntoh(struct webmsg *m)
{
	m->length   = le16tohs(m->length);
	m->sequence = le32tohl(m->sequence);
	m->ts.sec   = le32tohl(m->ts.sec);
	m->ts.nsec  = le32tohl(m->ts.nsec);
}

int webmsg_verify(struct webmsg *m)
{
	if      (m->version != WEBMSG_VERSION)
		return -1;
	else if (m->type    != WEBMSG_TYPE_DATA)
		return -2;
	else if ((m->rsvd1 != 0)  || (m->rsvd2 != 0))
		return -3;
	else
		return 0;
}