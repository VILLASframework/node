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

#include <string.h>

#include "io/msg.h"
#include "io/msg_format.h"
#include "sample.h"
#include "utils.h"
#include "plugin.h"

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
	else if (m->rsvd1 != 0)
		return -3;
	else
		return 0;
}

int msg_to_sample(struct msg *msg, struct sample *smp)
{
	int ret;

	ret = msg_verify(msg);
	if (ret)
		return -1;

	smp->length = MIN(msg->length, smp->capacity);
	smp->sequence = msg->sequence;
	smp->id = msg->id;
	smp->ts.origin = MSG_TS(msg);
	smp->ts.received.tv_sec  = -1;
	smp->ts.received.tv_nsec = -1;
	smp->format = 0;

	for (int i = 0; i < smp->length; i++) {
		switch (sample_get_data_format(smp, i)) {
			case SAMPLE_DATA_FORMAT_FLOAT: smp->data[i].f = msg->data[i].f; break;
			case SAMPLE_DATA_FORMAT_INT:   smp->data[i].i = msg->data[i].i; break;
		}
	}

	return 0;
}

int msg_from_sample(struct msg *msg, struct sample *smp)
{
	*msg = MSG_INIT(smp->length, smp->sequence);

	msg->ts.sec  = smp->ts.origin.tv_sec;
	msg->ts.nsec = smp->ts.origin.tv_nsec;
	msg->id = smp->id;

	for (int i = 0; i < smp->length; i++) {
		switch (sample_get_data_format(smp, i)) {
			case SAMPLE_DATA_FORMAT_FLOAT: msg->data[i].f = smp->data[i].f; break;
			case SAMPLE_DATA_FORMAT_INT:   msg->data[i].i = smp->data[i].i; break;
		}
	}

	return 0;
}

int msg_sprint(char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt, int flags)
{
	int ret, i = 0;
	char *ptr = buf;

	for (i = 0; i < cnt; i++) {
		struct msg *msg = (struct msg *) ptr;
		struct sample *smp = smps[i];
		
		if (ptr + MSG_LEN(smp->length) > buf + len)
			break;

		ret = msg_from_sample(msg, smp);
		if (ret)
			return ret;

		if (flags & MSG_WEB) {
			/** @todo convert to little endian */
		}
		else
			msg_hton(msg);

		ptr += MSG_LEN(smp->length);
	}
	
	if (wbytes)
		*wbytes = ptr - buf;

	return i;
}

int msg_sscan(char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt, int *flags)
{
	int ret, i = 0, values;
	char *ptr = buf;
	
	if (len % 4 != 0) {
		warn("Packet size is invalid: %zd Must be multiple of 4 bytes.", len);
		return -1;
	}

	for (i = 0; i < cnt; i++) {
		struct msg *msg = (struct msg *) ptr;
		struct sample *smp = smps[i];

		/* Complete buffer has been parsed */
		if (ptr == buf + len)
			break;

		/* Check if header is still in buffer bounaries */
		if (ptr + sizeof(struct msg) > buf + len) {
			warn("Invalid msg received: reason=1");
			break;
		}
		
		values = (*flags & MSG_WEB) ? msg->length : ntohs(msg->length);
		
		/* Check if remainder of message is in buffer boundaries */
		if (ptr + MSG_LEN(values) > buf + len) {
			warn("Invalid msg received: reason=2, msglen=%zu, len=%zu, ptr=%p, buf=%p, i=%u", MSG_LEN(values), len, ptr, buf, i);
			break;
		}

		if (*flags & MSG_WEB)
			;
		else
			msg_ntoh(msg);

		ret = msg_to_sample(msg, smp);
		if (ret) {
			warn("Invalid msg received: reason=3, ret=%d", ret);
			break;
		}

		ptr += MSG_LEN(smp->length);
	}
	
	if (rbytes)
		*rbytes = ptr - buf;

	return i;
}

static struct plugin p1 = {
	.name = "msg",
	.description = "VILLAS binary network format",
	.type = PLUGIN_TYPE_IO,
	.io = {
		.sprint	= msg_sprint,
		.sscan	= msg_sscan,
		.size	= 0,
		.flags	= IO_FORMAT_BINARY
	},
};

/** The WebSocket node-type usually uses little endian byte order intead of network byte order */
static struct plugin p2 = {
	.name = "webmsg",
	.description = "VILLAS binary network format for WebSockets",
	.type = PLUGIN_TYPE_IO,
	.io = {
		.sprint	= msg_sprint,
		.sscan	= msg_sscan,
		.size	= 0,
		.flags	= IO_FORMAT_BINARY | MSG_WEB
	},
};

REGISTER_PLUGIN(&p1);
REGISTER_PLUGIN(&p2);
