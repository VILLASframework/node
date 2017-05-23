/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <arpa/inet.h>
#include <string.h>

#include "msg.h"
#include "msg_format.h"
#include "sample.h"
#include "utils.h"

void msg_ntoh(struct msg *m)
{
	msg_hdr_ntoh(m);

	for (int i = 0; i < m->length; i++)
		m->data[i].i = ntohl(m->data[i].i);
}

void msg_hton(struct msg *m)
{
	for (int i = 0; i < m->length; i++)
		m->data[i].i = htonl(m->data[i].i);

	msg_hdr_hton(m);
}

void msg_hdr_hton(struct msg *m)
{
	m->length   = htons(m->length);
	m->sequence = htonl(m->sequence);
	m->ts.sec   = htonl(m->ts.sec);
	m->ts.nsec  = htonl(m->ts.nsec);
}

void msg_hdr_ntoh(struct msg *m)
{
	m->length   = ntohs(m->length);
	m->sequence = ntohl(m->sequence);
	m->ts.sec   = ntohl(m->ts.sec);
	m->ts.nsec  = ntohl(m->ts.nsec);
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

int msg_to_sample(struct msg *msg, struct sample *smp)
{
	int ret;

	msg_ntoh(msg);

	ret = msg_verify(msg);
	if (ret)
		return -1;

	smp->length = MIN(msg->length, smp->capacity);
	smp->sequence = msg->sequence;
	smp->ts.origin = MSG_TS(msg);
	smp->ts.received.tv_sec  = -1;
	smp->ts.received.tv_nsec = -1;

	memcpy(smp->data, msg->data, SAMPLE_DATA_LEN(smp->length));
	
	return 0;
}

int msg_from_sample(struct msg *msg, struct sample *smp)
{
	*msg = MSG_INIT(smp->length, smp->sequence);

	msg->ts.sec  = smp->ts.origin.tv_sec;
	msg->ts.nsec = smp->ts.origin.tv_nsec;

	memcpy(msg->data, smp->data, MSG_DATA_LEN(smp->length));

	msg_hton(msg);

	return 0;
}

ssize_t msg_buffer_from_samples(struct sample *smps[], unsigned cnt, char *buf, size_t len)
{
	int ret, i = 0;
	char *ptr = buf;

	struct msg *msg = (struct msg *) ptr;
	struct sample *smp = smps[i];

	while (ptr < buf + len && i < cnt) {
		ret = msg_from_sample(msg, smp);
		if (ret)
			return ret;

		ptr += MSG_LEN(smp->length);

		msg = (struct msg *) ptr;
		smp = smps[++i];
	}
	
	return ptr - buf;
}

int msg_buffer_to_samples(struct sample *smps[], unsigned cnt, char *buf, size_t len)
{
	int ret, i = 0;
	char *ptr = buf;

	struct msg *msg = (struct msg *) ptr;
	struct sample *smp = smps[i];

	while (ptr < buf + len && i < cnt) {
		ret = msg_to_sample(msg, smp);
		if (ret)
			return ret;

		ptr += MSG_LEN(smp->length);

		msg = (struct msg *) ptr;
		smp = smps[++i];
	}
	
	return i;
}