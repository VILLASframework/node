/* Message related functions.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <arpa/inet.h>

#include <villas/formats/msg.hpp>
#include <villas/formats/msg_format.hpp>
#include <villas/sample.hpp>
#include <villas/signal.hpp>
#include <villas/utils.hpp>
#include <villas/list.hpp>

using namespace villas;
using namespace villas::node;

void villas::node::msg_ntoh(struct Message *m)
{
	msg_hdr_ntoh(m);

	for (int i = 0; i < m->length; i++)
		m->data[i].i = ntohl(m->data[i].i);
}

void villas::node::msg_hton(struct Message *m)
{
	for (int i = 0; i < m->length; i++)
		m->data[i].i = htonl(m->data[i].i);

	msg_hdr_hton(m);
}

void villas::node::msg_hdr_hton(struct Message *m)
{
	m->length   = htons(m->length);
	m->sequence = htonl(m->sequence);
	m->ts.sec   = htonl(m->ts.sec);
	m->ts.nsec  = htonl(m->ts.nsec);
}

void villas::node::msg_hdr_ntoh(struct Message *m)
{
	m->length   = ntohs(m->length);
	m->sequence = ntohl(m->sequence);
	m->ts.sec   = ntohl(m->ts.sec);
	m->ts.nsec  = ntohl(m->ts.nsec);
}

int villas::node::msg_verify(const struct Message *m)
{
	if      (m->version != MSG_VERSION)
		return -1;
	else if (m->type != MSG_TYPE_DATA)
		return -2;
	else if (m->reserved1 != 0)
		return -3;
	else
		return 0;
}

int villas::node::msg_to_sample(const struct Message *msg, struct Sample *smp, const SignalList::Ptr sigs, uint8_t *source_index)
{
	int ret;
	unsigned i;

	ret = msg_verify(msg);
	if (ret)
		return ret;

	unsigned len = MIN(msg->length, smp->capacity);
	for (i = 0; i < MIN(len, sigs->size()); i++) {
		auto sig = sigs->getByIndex(i);
		if (!sig)
			return -1;

		switch (sig->type) {
			case SignalType::FLOAT:
				smp->data[i].f = msg->data[i].f;
				break;

			case SignalType::INTEGER:
				smp->data[i].i = msg->data[i].i;
				break;

			default:
				return -1;
		}
	}

	smp->flags = (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA;
	smp->length = i;
	smp->sequence = msg->sequence;
	MSG_TS(msg, smp->ts.origin);

	if (source_index)
		*source_index = msg->source_index;

	return 0;
}

int villas::node::msg_from_sample(struct Message *msg_in, const struct Sample *smp, const SignalList::Ptr sigs, uint8_t source_index)
{
	msg_in->type     = MSG_TYPE_DATA;
	msg_in->version  = MSG_VERSION;
	msg_in->reserved1 = 0;
	msg_in->source_index = source_index;
	msg_in->length   = (uint16_t) smp->length;
	msg_in->sequence = (uint32_t) smp->sequence;
	msg_in->ts.sec  = smp->ts.origin.tv_sec;
	msg_in->ts.nsec = smp->ts.origin.tv_nsec;

	for (unsigned i = 0; i < smp->length; i++) {
		auto sig = sigs->getByIndex(i);
		if (!sig)
			return -1;

		switch (sig->type) {
			case SignalType::FLOAT:
				msg_in->data[i].f = smp->data[i].f;
				break;

			case SignalType::INTEGER:
				msg_in->data[i].i = smp->data[i].i;
				break;

			default:
				return -1;
		}
	}

	return 0;
}
