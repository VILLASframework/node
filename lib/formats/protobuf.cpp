/** Protobuf IO format
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

/* Generated message descriptors by protoc */
#include <villas.pb-c.h>

#include <villas/sample.h>
#include <villas/signal.h>
#include <villas/io.h>
#include <villas/plugin.h>
#include <villas/formats/protobuf.h>

using namespace villas::utils;

static enum SignalType protobuf_detect_format(Villas__Node__Value *val)
{
	switch (val->value_case) {
		case VILLAS__NODE__VALUE__VALUE_F:
			return SignalType::FLOAT;

		case VILLAS__NODE__VALUE__VALUE_I:
			return SignalType::INTEGER;

		case VILLAS__NODE__VALUE__VALUE_B:
			return SignalType::BOOLEAN;

		case VILLAS__NODE__VALUE__VALUE_Z:
			return SignalType::COMPLEX;

		case VILLAS__NODE__VALUE__VALUE__NOT_SET:
		default:
			return SignalType::INVALID;
	}
}

int protobuf_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	unsigned psz;

	Villas__Node__Message *pb_msg = (Villas__Node__Message *) alloc(sizeof(Villas__Node__Message));
	villas__node__message__init(pb_msg);

	pb_msg->n_samples = cnt;
	pb_msg->samples = (Villas__Node__Sample **) alloc(pb_msg->n_samples * sizeof(Villas__Node__Sample *));

	for (unsigned i = 0; i < pb_msg->n_samples; i++) {
		Villas__Node__Sample *pb_smp = pb_msg->samples[i] = (Villas__Node__Sample *) alloc(sizeof(Villas__Node__Sample));
		villas__node__sample__init(pb_smp);

		struct sample *smp = smps[i];

		pb_smp->type = VILLAS__NODE__SAMPLE__TYPE__DATA;

		if (io->flags & smp->flags & (int) SampleFlags::HAS_SEQUENCE) {
			pb_smp->has_sequence = 1;
			pb_smp->sequence = smp->sequence;
		}

		if (io->flags & smp->flags & (int) SampleFlags::HAS_TS_ORIGIN) {
			pb_smp->timestamp = (Villas__Node__Timestamp *) alloc(sizeof(Villas__Node__Timestamp));
			villas__node__timestamp__init(pb_smp->timestamp);

			pb_smp->timestamp->sec = smp->ts.origin.tv_sec;
			pb_smp->timestamp->nsec = smp->ts.origin.tv_nsec;
		}

		pb_smp->n_values = smp->length;
		pb_smp->values = (Villas__Node__Value **) alloc(pb_smp->n_values * sizeof(Villas__Node__Value *));

		for (unsigned j = 0; j < pb_smp->n_values; j++) {
			Villas__Node__Value *pb_val = pb_smp->values[j] = (Villas__Node__Value *) alloc(sizeof(Villas__Node__Value));
			villas__node__value__init(pb_val);

			enum SignalType fmt = sample_format(smp, j);
			switch (fmt) {
				case SignalType::FLOAT:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE_F;
					pb_val->f = smp->data[j].f;
					break;

				case SignalType::INTEGER:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE_I;
					pb_val->i = smp->data[j].i;
					break;

				case SignalType::BOOLEAN:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE_B;
					pb_val->b = smp->data[j].b;
					break;

				case SignalType::COMPLEX:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE_Z;
					pb_val->z = (Villas__Node__Complex *) alloc(sizeof(Villas__Node__Complex));

					villas__node__complex__init(pb_val->z);

					pb_val->z->real = std::real(smp->data[j].z);
					pb_val->z->imag = std::imag(smp->data[j].z);
					break;

				case SignalType::INVALID:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE__NOT_SET;
					break;
			}
		}
	}

	psz = villas__node__message__get_packed_size(pb_msg);

	if (psz > len)
		goto out;

	villas__node__message__pack(pb_msg, (uint8_t *) buf);
	villas__node__message__free_unpacked(pb_msg, nullptr);

	*wbytes = psz;

	return cnt;

out:
	villas__node__message__free_unpacked(pb_msg, nullptr);

	return -1;
}

int protobuf_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	unsigned i, j;
	Villas__Node__Message *pb_msg;

	pb_msg = villas__node__message__unpack(nullptr, len, (uint8_t *) buf);
	if (!pb_msg)
		return -1;

	for (i = 0; i < MIN(pb_msg->n_samples, cnt); i++) {
		struct sample *smp = smps[i];
		Villas__Node__Sample *pb_smp = pb_msg->samples[i];

		smp->flags = 0;
		smp->signals = io->signals;

		if (pb_smp->type != VILLAS__NODE__SAMPLE__TYPE__DATA) {
			warning("Parsed non supported message type. Skipping");
			continue;
		}

		if (pb_smp->has_sequence) {
			smp->flags |= (int) SampleFlags::HAS_SEQUENCE;
			smp->sequence = pb_smp->sequence;
		}

		if (pb_smp->timestamp) {
			smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;
			smp->ts.origin.tv_sec = pb_smp->timestamp->sec;
			smp->ts.origin.tv_nsec = pb_smp->timestamp->nsec;
		}

		for (j = 0; j < MIN(pb_smp->n_values, smp->capacity); j++) {
			Villas__Node__Value *pb_val = pb_smp->values[j];

			enum SignalType fmt = protobuf_detect_format(pb_val);

			struct signal *sig = (struct signal *) vlist_at_safe(smp->signals, j);
			if (!sig)
				return -1;

			if (sig->type != fmt) {
				error("Received invalid data type in Protobuf payload: Received %s, expected %s for signal %s (index %u).",
					signal_type_to_str(fmt), signal_type_to_str(sig->type), sig->name, i);
				return -2;
			}

			switch (sig->type) {
				case SignalType::FLOAT:
					smp->data[j].f = pb_val->f;
					break;

				case SignalType::INTEGER:
					smp->data[j].i = pb_val->i;
					break;

				case SignalType::BOOLEAN:
					smp->data[j].b = pb_val->b;
					break;

				case SignalType::COMPLEX:
					smp->data[j].z = std::complex<float>(pb_val->z->real, pb_val->z->imag);
					break;

				default: { }
			}
		}

		if (pb_smp->n_values > 0)
			smp->flags |= (int) SampleFlags::HAS_DATA;

		smp->length = j;
	}

	if (rbytes)
		*rbytes = villas__node__message__get_packed_size(pb_msg);

	villas__node__message__free_unpacked(pb_msg, nullptr);

	return i;
}

static struct plugin p;

__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	if (plugins.state == State::DESTROYED)
		vlist_init(&plugins);

	p.name = "protobuf";
	p.description = "Google Protobuf";
	p.type = PluginType::FORMAT;
	p.format.sprint	= protobuf_sprint;
	p.format.sscan = protobuf_sscan;
	p.format.flags = (int) IOFlags::HAS_BINARY_PAYLOAD |
		          (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA;

	vlist_push(&plugins, &p);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
	if (plugins.state != State::DESTROYED)
		vlist_remove_all(&plugins, &p);
}

