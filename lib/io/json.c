/** JSON serializtion of sample data.
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

#include "plugin.h"
#include "io/json.h"

int json_pack_sample(json_t **j, struct sample *smp, int flags)
{
	json_t *json_smp;
	json_error_t err;

	json_smp = json_pack_ex(&err, 0, "{ s: { s: [ I, I ], s: [ I, I ], s: [ I, I ] } }",
		"ts",
			"origin",   smp->ts.origin.tv_sec,   smp->ts.origin.tv_nsec,
			"received", smp->ts.received.tv_sec, smp->ts.received.tv_nsec,
			"sent",     smp->ts.sent.tv_sec,     smp->ts.sent.tv_nsec);

	if (flags & SAMPLE_SEQUENCE) {
		json_t *json_sequence = json_integer(smp->sequence);

		json_object_set(json_smp, "sequence", json_sequence);
	}

	if (flags & SAMPLE_VALUES) {
		json_t *json_data = json_array();

		for (int i = 0; i < smp->length; i++) {
			json_t *json_value = sample_get_data_format(smp, i)
						? json_integer(smp->data[i].i)
						: json_real(smp->data[i].f);

			json_array_append(json_data, json_value);
		}

		json_object_set(json_smp, "data", json_data);
	}

	*j = json_smp;

	return 0;
}

int json_pack_samples(json_t **j, struct sample *smps[], unsigned cnt, int flags)
{
	int ret;
	json_t *json_smps = json_array();

	for (int i = 0; i < cnt; i++) {
		json_t *json_smp;

		ret = json_pack_sample(&json_smp, smps[i], flags);
		if (ret)
			break;

		json_array_append(json_smps, json_smp);
	}

	*j = json_smps;

	return cnt;
}

int json_unpack_sample(json_t *json_smp, struct sample *smp, int flags)
{
	int ret;
	json_t *json_data, *json_value;
	size_t i;

	ret = json_unpack(json_smp, "{ s: { s: [ I, I ], s: [ I, I ], s: [ I, I ] }, s: I, s: o }",
		"ts",
			"origin",   &smp->ts.origin.tv_sec,   &smp->ts.origin.tv_nsec,
			"received", &smp->ts.received.tv_sec, &smp->ts.received.tv_nsec,
			"sent",     &smp->ts.sent.tv_sec,     &smp->ts.sent.tv_nsec,
		"sequence", &smp->sequence,
		"data", &json_data);

	if (ret)
		return ret;

	if (!json_is_array(json_data))
		return -1;

	smp->has = SAMPLE_ORIGIN | SAMPLE_RECEIVED | SAMPLE_SEQUENCE;
	smp->length = 0;

	json_array_foreach(json_data, i, json_value) {
		if (i >= smp->capacity)
			break;

		switch (json_typeof(json_value)) {
			case JSON_REAL:
				smp->data[i].f = json_real_value(json_value);
				sample_set_data_format(smp, i, SAMPLE_DATA_FORMAT_FLOAT);
				break;

			case JSON_INTEGER:
				smp->data[i].f = json_integer_value(json_value);
				sample_set_data_format(smp, i, SAMPLE_DATA_FORMAT_INT);
				break;

			default:
				return -2;
		}

		smp->length++;
	}

	if (smp->length > 0)
		smp->has |= SAMPLE_VALUES;

	return 0;
}

int json_unpack_samples(json_t *json_smps, struct sample *smps[], unsigned cnt, int flags)
{
	int ret;
	json_t *json_smp;
	size_t i;

	if (!json_is_array(json_smps))
		return -1;

	json_array_foreach(json_smps, i, json_smp) {
		if (i >= cnt)
			break;

		ret = json_unpack_sample(json_smp, smps[i], flags);
		if (ret < 0)
			break;
	}

	return i;
}

int json_sprint(char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt, int flags)
{
	int ret;
	json_t *json;
	size_t wr;

	ret = json_pack_samples(&json, smps, cnt, flags);
	if (ret < 0)
		return ret;

	wr = json_dumpb(json, buf, len, 0);

	json_decref(json);

	if (wbytes)
		*wbytes = wr;

	return ret;
}

int json_sscan(char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt, int flags)
{
	int ret;
	json_t *json;
	json_error_t err;

	json = json_loadb(buf, len, 0, &err);
	if (!json)
		return -1;

	ret = json_unpack_samples(json, smps, cnt, flags);
	if (ret < 0)
		return ret;

	json_decref(json);

	if (rbytes)
		*rbytes = err.position;

	return ret;
}

int json_fprint(FILE *f, struct sample *smps[], unsigned cnt, int flags)
{
	int ret, i;
	json_t *json;

	for (i = 0; i < cnt; i++) {
		ret = json_pack_sample(&json, smps[i], flags);
		if (ret)
			return ret;

		ret = json_dumpf(json, f, 0);
		fputc('\n', f);

		json_decref(json);
	}

	return i;
}

int json_fscan(FILE *f, struct sample *smps[], unsigned cnt, int flags)
{
	int i, ret;
	json_t *json;
	json_error_t err;

	for (i = 0; i < cnt; i++) {
skip:		json = json_loadf(f, JSON_DISABLE_EOF_CHECK, &err);
		if (!json)
			break;

		ret = json_unpack_sample(json, smps[i], flags);
		if (ret)
			goto skip;

		json_decref(json);
	}

	return i;
}

static struct plugin p = {
	.name = "json",
	.description = "Javascript Object Notation",
	.type = PLUGIN_TYPE_IO,
	.io = {
		.fscan	= json_fscan,
		.fprint	= json_fprint,
		.sscan	= json_sscan,
		.sprint	= json_sprint,
		.size = 0
	},
};

REGISTER_PLUGIN(&p);
