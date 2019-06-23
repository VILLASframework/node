/** Message related functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/io.h>
#include <villas/formats/villas_binary.h>
#include <villas/formats/msg.h>
#include <villas/formats/msg_format.h>
#include <villas/sample.h>
#include <villas/utils.hpp>
#include <villas/plugin.h>

int villas_binary_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	int ret;
	unsigned i = 0;
	char *ptr = buf;

	for (i = 0; i < cnt; i++) {
		struct msg *msg = (struct msg *) ptr;
		struct sample *smp = smps[i];

		if (ptr + MSG_LEN(smp->length) > buf + len)
			break;

		ret = msg_from_sample(msg, smp, smp->signals);
		if (ret)
			return ret;

		if (io->flags & VILLAS_BINARY_WEB) {
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

int villas_binary_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	int ret, values;
	unsigned i = 0;
	const char *ptr = buf;

	if (len % 4 != 0) {
		warning("Packet size is invalid: %zd Must be multiple of 4 bytes.", len);
		return -1;
	}

	for (i = 0; i < cnt; i++) {
		struct msg *msg = (struct msg *) ptr;
		struct sample *smp = smps[i];

		smp->signals = io->signals;

		/* Complete buffer has been parsed */
		if (ptr == buf + len)
			break;

		/* Check if header is still in buffer bounaries */
		if (ptr + sizeof(struct msg) > buf + len) {
			warning("Invalid msg received: reason=1");
			break;
		}

		values = (io->flags & VILLAS_BINARY_WEB) ? msg->length : ntohs(msg->length);

		/* Check if remainder of message is in buffer boundaries */
		if (ptr + MSG_LEN(values) > buf + len) {
			warning("Invalid msg received: reason=2, msglen=%zu, len=%zu, ptr=%p, buf=%p, i=%u", MSG_LEN(values), len, ptr, buf, i);
			break;
		}

		if (io->flags & VILLAS_BINARY_WEB) {
			/** @todo convert from little endian */
		}
		else
			msg_ntoh(msg);

		ret = msg_to_sample(msg, smp, io->signals);
		if (ret) {
			warning("Invalid msg received: reason=3, ret=%d", ret);
			break;
		}

		ptr += MSG_LEN(smp->length);
	}

	if (rbytes)
		*rbytes = ptr - buf;

	return i;
}

static struct plugin p1;

__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	if (plugins.state == State::DESTROYED)
	        vlist_init(&plugins);

	p1.name = "villas.binary";
	p1.description = "VILLAS binary network format";
	p1.type = PluginType::FORMAT;
	p1.format.sprint	= villas_binary_sprint;
	p1.format.sscan	= villas_binary_sscan;
	p1.format.size	= 0;
	p1.format.flags	= (int) IOFlags::HAS_BINARY_PAYLOAD |
		          (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA;

	vlist_push(&plugins, &p1);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p1);
}
/** The WebSocket node-type usually uses little endian byte order intead of network byte order */
static struct plugin p2;


__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	if (plugins.state == State::DESTROYED)
	        vlist_init(&plugins);

	p2.name = "villas.web";
	p2.description = "VILLAS binary network format for WebSockets";
	p2.type = PluginType::FORMAT;
	p2.format.sprint	= villas_binary_sprint;
	p2.format.sscan	= villas_binary_sscan;
	p2.format.size	= 0;
	p2.format.flags	= (int) IOFlags::HAS_BINARY_PAYLOAD | VILLAS_BINARY_WEB |
		          (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA;

	vlist_push(&plugins, &p2);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p2);
}
