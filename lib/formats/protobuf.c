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

static enum signal_type protobuf_detect_format(Villas__Node__Value *val)
{
	switch (val->value_case) {
		case VILLAS__NODE__VALUE__VALUE_F:
			return SIGNAL_TYPE_FLOAT;

		case VILLAS__NODE__VALUE__VALUE_I:
			return SIGNAL_TYPE_INTEGER;

		case VILLAS__NODE__VALUE__VALUE_B:
			return SIGNAL_TYPE_BOOLEAN;

		case VILLAS__NODE__VALUE__VALUE_Z:
			return SIGNAL_TYPE_COMPLEX;

		case VILLAS__NODE__VALUE__VALUE__NOT_SET:
		default:
			return SIGNAL_TYPE_INVALID;
	}
}

int protobuf_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	unsigned psz;

	Villas__Node__Message *pb_msg = alloc(sizeof(Villas__Node__Message));
	villas__node__message__init(pb_msg);

	pb_msg->n_samples = cnt;
	pb_msg->samples = alloc(pb_msg->n_samples * sizeof(Villas__Node__Sample *));

	for (unsigned i = 0; i < pb_msg->n_samples; i++) {
		Villas__Node__Sample *pb_smp = pb_msg->samples[i] = alloc(sizeof(Villas__Node__Sample));
		villas__node__sample__init(pb_smp);

		struct sample *smp = smps[i];

		pb_smp->type = VILLAS__NODE__SAMPLE__TYPE__DATA;

		if (io->flags & smp->flags & SAMPLE_HAS_SEQUENCE) {
			pb_smp->has_sequence = 1;
			pb_smp->sequence = smp->sequence;
		}

		if (io->flags & smp->flags & SAMPLE_HAS_TS_ORIGIN) {
			pb_smp->timestamp = alloc(sizeof(Villas__Node__Timestamp));
			villas__node__timestamp__init(pb_smp->timestamp);

			pb_smp->timestamp->sec = smp->ts.origin.tv_sec;
			pb_smp->timestamp->nsec = smp->ts.origin.tv_nsec;
		}

		pb_smp->n_values = smp->length;
		pb_smp->values = alloc(pb_smp->n_values * sizeof(Villas__Node__Value *));

		for (unsigned j = 0; j < pb_smp->n_values; j++) {
			Villas__Node__Value *pb_val = pb_smp->values[j] = alloc(sizeof(Villas__Node__Value));
			villas__node__value__init(pb_val);

			enum signal_type fmt = sample_format(smp, j);
			switch (fmt) {
				case SIGNAL_TYPE_FLOAT:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE_F;
					pb_val->f = smp->data[j].f;
					break;

				case SIGNAL_TYPE_INTEGER:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE_I;
					pb_val->i = smp->data[j].i;
					break;

				case SIGNAL_TYPE_BOOLEAN:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE_B;
					pb_val->b = smp->data[j].b;
					break;

				case SIGNAL_TYPE_COMPLEX:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE_Z;
					pb_val->z = alloc(sizeof(Villas__Node__Complex));

					villas__node__complex__init(pb_val->z);

					pb_val->z->real = creal(smp->data[j].z);
					pb_val->z->imag = cimag(smp->data[j].z);
					break;

				case SIGNAL_TYPE_INVALID:
					pb_val->value_case = VILLAS__NODE__VALUE__VALUE__NOT_SET;
					break;
			}
		}
	}

	psz = villas__node__message__get_packed_size(pb_msg);

	if (psz > len)
		goto out;

	villas__node__message__pack(pb_msg, (uint8_t *) buf);
	villas__node__message__free_unpacked(pb_msg, NULL);

	*wbytes = psz;

	return cnt;

out:
	villas__node__message__free_unpacked(pb_msg, NULL);

	return -1;
}

int protobuf_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	unsigned i, j;
	Villas__Node__Message *pb_msg;

	pb_msg = villas__node__message__unpack(NULL, len, (uint8_t *) buf);
	if (!pb_msg)
		return -1;

	for (i = 0; i < MIN(pb_msg->n_samples, cnt); i++) {
		struct sample *smp = smps[i];
		Villas__Node__Sample *pb_smp = pb_msg->samples[i];

		smp->signals = io->signals;

		if (pb_smp->type != VILLAS__NODE__SAMPLE__TYPE__DATA) {
			warning("Parsed non supported message type. Skipping");
			continue;
		}

		if (pb_smp->has_sequence) {
			smp->flags |= SAMPLE_HAS_SEQUENCE;
			smp->sequence = pb_smp->sequence;
		}

		if (pb_smp->timestamp) {
			smp->flags |= SAMPLE_HAS_TS_ORIGIN;
			smp->ts.origin.tv_sec = pb_smp->timestamp->sec;
			smp->ts.origin.tv_nsec = pb_smp->timestamp->nsec;
		}

		for (j = 0; j < MIN(pb_smp->n_values, smp->capacity); j++) {
			Villas__Node__Value *pb_val = pb_smp->values[j];

			enum signal_type fmt = protobuf_detect_format(pb_val);

			struct signal *sig = (struct signal *) vlist_at_safe(smp->signals, j);
			if (!sig)
				return -1;

			if (sig->type != fmt) {
				error("Received invalid data type in Protobuf payload: Received %s, expected %s for signal %s (index %u).",
					signal_type_to_str(fmt), signal_type_to_str(sig->type), sig->name, i);
				return -2;
			}

			switch (sig->type) {
				case SIGNAL_TYPE_FLOAT:
					smp->data[j].f = pb_val->f;
					break;

				case SIGNAL_TYPE_INTEGER:
					smp->data[j].i = pb_val->i;
					break;

				case SIGNAL_TYPE_BOOLEAN:
					smp->data[j].b = pb_val->b;
					break;

				case SIGNAL_TYPE_COMPLEX:
					smp->data[j].z = CMPLXF(pb_val->z->real, pb_val->z->imag);
					break;

				default: { }
			}
		}

		if (pb_smp->n_values > 0)
			smp->flags |= SAMPLE_HAS_DATA;

		smp->length = j;
	}

	if (rbytes)
		*rbytes = villas__node__message__get_packed_size(pb_msg);

	villas__node__message__free_unpacked(pb_msg, NULL);

	return i;
}

static struct plugin p = {
	.name = "protobuf",
	.description = "Google Protobuf",
	.type = PLUGIN_TYPE_FORMAT,
	.format = {
		.sprint	= protobuf_sprint,
		.sscan	= protobuf_sscan,
		.flags	= IO_HAS_BINARY_PAYLOAD |
		          SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_DATA
	}
};
REGISTER_PLUGIN(&p);
