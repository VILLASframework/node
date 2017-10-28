/** Protobuf IO format
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

// Generated message descriptors by protoc
#include "villas.pb-c.h"

#include "sample.h"
#include "plugin.h"
#include "io/protobuf.h"

int protobuf_sprint(char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt, int flags)
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

		if (smp->flags & SAMPLE_HAS_SEQUENCE) {
			pb_smp->has_sequence = 1;
			pb_smp->sequence = smp->sequence;
		}

		if (smp->flags & SAMPLE_HAS_ID) {
			pb_smp->has_id = 1;
			pb_smp->id = smp->id;
		}

		if (smp->flags & SAMPLE_HAS_ORIGIN) {
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

			enum sample_data_format fmt = sample_get_data_format(smp, j);

			switch (fmt) {
				case SAMPLE_DATA_FORMAT_FLOAT:	pb_val->value_case = VILLAS__NODE__VALUE__VALUE_F; pb_val->f = smp->data[j].f;	break;
				case SAMPLE_DATA_FORMAT_INT:	pb_val->value_case = VILLAS__NODE__VALUE__VALUE_I; pb_val->i = smp->data[j].i;	break;
				default:			pb_val->value_case = VILLAS__NODE__VALUE__VALUE__NOT_SET;			break;
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

int protobuf_sscan(char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt, int flags)
{
	unsigned i, j;
	Villas__Node__Message *pb_msg;

	pb_msg = villas__node__message__unpack(NULL, len, (uint8_t *) buf);

	for (i = 0; i < MIN(pb_msg->n_samples, cnt); i++) {
		struct sample *smp = smps[i];
		Villas__Node__Sample *pb_smp = pb_msg->samples[i];

		smp->flags = SAMPLE_HAS_FORMAT;

		if (pb_smp->type != VILLAS__NODE__SAMPLE__TYPE__DATA) {
			warn("Parsed non supported message type");
			break;
		}

		if (pb_smp->has_sequence) {
			smp->flags |= SAMPLE_HAS_SEQUENCE;
			smp->sequence = pb_smp->sequence;
		}

		if (pb_smp->has_id) {
			smp->flags |= SAMPLE_HAS_ID;
			smp->id	= pb_smp->id;
		}

		if (pb_smp->timestamp) {
			smp->flags |= SAMPLE_HAS_ORIGIN;
			smp->ts.origin.tv_sec = pb_smp->timestamp->sec;
			smp->ts.origin.tv_nsec = pb_smp->timestamp->nsec;
		}

		for (j = 0; j < MIN(pb_smp->n_values, smp->capacity); j++) {
			Villas__Node__Value *pb_val = pb_smp->values[j];

			enum sample_data_format fmt = pb_val->value_case == VILLAS__NODE__VALUE__VALUE_F
							? SAMPLE_DATA_FORMAT_FLOAT
							: SAMPLE_DATA_FORMAT_INT;

			switch (fmt) {
				case SAMPLE_DATA_FORMAT_FLOAT:	smp->data[j].f = pb_val->f; break;
				case SAMPLE_DATA_FORMAT_INT:	smp->data[j].i = pb_val->i; break;
				default: { }
			}

			sample_set_data_format(smp, j, fmt);
		}

		if (pb_smp->n_values > 0)
			smp->flags |= SAMPLE_HAS_VALUES;

		smp->length = j;
	}

	*rbytes = villas__node__message__get_packed_size(pb_msg);

	villas__node__message__free_unpacked(pb_msg, NULL);

	return i;
}

static struct plugin p = {
	.name = "protobuf",
	.description = "Google Protobuf",
	.type = PLUGIN_TYPE_IO,
	.io = {
		.sprint = protobuf_sprint,
		.sscan  = protobuf_sscan
	}
};
REGISTER_PLUGIN(&p);
