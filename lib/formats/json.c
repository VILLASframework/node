/** JSON serializtion of sample data.
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

#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/compat.h>
#include <villas/signal.h>
#include <villas/io.h>
#include <villas/formats/json.h>

static enum signal_type json_detect_format(json_t *val)
{
	int type = json_typeof(val);

	switch (type) {
		case JSON_REAL:
			return SIGNAL_TYPE_FLOAT;

		case JSON_INTEGER:
			return SIGNAL_TYPE_INTEGER;

		case JSON_TRUE:
		case JSON_FALSE:
			return SIGNAL_TYPE_BOOLEAN;

		case JSON_OBJECT:
			return SIGNAL_TYPE_COMPLEX; /* must be a complex number */

		default:
			return SIGNAL_TYPE_AUTO;
	}
}

static json_t * json_pack_timestamps(struct io *io, struct sample *smp)
{
	json_t *json_ts = json_object();

	if (io->flags & smp->flags & SAMPLE_HAS_TS_ORIGIN)
		json_object_set(json_ts, "origin", json_pack("[ I, I ]", smp->ts.origin.tv_sec, smp->ts.origin.tv_nsec));

	if (io->flags & smp->flags & SAMPLE_HAS_TS_RECEIVED)
		json_object_set(json_ts, "received", json_pack("[ I, I ]", smp->ts.received.tv_sec, smp->ts.received.tv_nsec));

	return json_ts;
}

static int json_unpack_timestamps(json_t *json_ts, struct sample *smp)
{
	int ret;
	json_error_t err;
	json_t *json_ts_origin = NULL, *json_ts_received = NULL;

	json_unpack_ex(json_ts, &err, 0, "{ s?: o, s?: o }",
		"origin",   &json_ts_origin,
		"received", &json_ts_received
	);

	if (json_ts_origin) {
		ret = json_unpack_ex(json_ts_origin, &err, 0, "[ I, I ]", &smp->ts.origin.tv_sec, &smp->ts.origin.tv_nsec);
		if (ret)
			return ret;

		smp->flags |= SAMPLE_HAS_TS_ORIGIN;
	}

	if (json_ts_received) {
		ret = json_unpack_ex(json_ts_received, &err, 0, "[ I, I ]", &smp->ts.received.tv_sec, &smp->ts.received.tv_nsec);
		if (ret)
			return ret;

		smp->flags |= SAMPLE_HAS_TS_RECEIVED;
	}

	return 0;
}

static int json_pack_sample(struct io *io, json_t **j, struct sample *smp)
{
	json_t *json_smp;
	json_error_t err;

	json_smp = json_pack_ex(&err, 0, "{ s: o }", "ts", json_pack_timestamps(io, smp));

	if (io->flags & smp->flags & SAMPLE_HAS_SEQUENCE) {
		json_t *json_sequence = json_integer(smp->sequence);

		json_object_set(json_smp, "sequence", json_sequence);
	}

	if (io->flags & smp->flags & SAMPLE_HAS_DATA) {
		json_t *json_data = json_array();

		for (int i = 0; i < smp->length; i++) {
			enum signal_type fmt = sample_format(smp, i);

			json_t *json_value;
			switch (fmt) {
				case SIGNAL_TYPE_INTEGER:
					json_value = json_integer(smp->data[i].i);
					break;

				case SIGNAL_TYPE_FLOAT:
					json_value = json_real(smp->data[i].f);
					break;

				case SIGNAL_TYPE_BOOLEAN:
					json_value = json_boolean(smp->data[i].b);
					break;

				case SIGNAL_TYPE_COMPLEX:
					json_value = json_pack("{ s: f, s: f }",
						"real", creal(smp->data[i].z),
						"imag", cimag(smp->data[i].z)
					);
					break;

				case SIGNAL_TYPE_INVALID:
				case SIGNAL_TYPE_AUTO:
					json_value = json_null(); /* Unknown type */
					break;
			}

			json_array_append(json_data, json_value);
		}

		json_object_set(json_smp, "data", json_data);
	}

	*j = json_smp;

	return 0;
}

static int json_pack_samples(struct io *io, json_t **j, struct sample *smps[], unsigned cnt)
{
	int ret;
	json_t *json_smps = json_array();

	for (int i = 0; i < cnt; i++) {
		json_t *json_smp;

		ret = json_pack_sample(io, &json_smp, smps[i]);
		if (ret)
			break;

		json_array_append(json_smps, json_smp);
	}

	*j = json_smps;

	return cnt;
}

static int json_unpack_sample(struct io *io, json_t *json_smp, struct sample *smp)
{
	int ret;
	json_error_t err;
	json_t *json_data, *json_value, *json_ts = NULL;
	size_t i;
	int64_t sequence = -1;

	smp->signals = io->signals;

	ret = json_unpack_ex(json_smp, &err, 0, "{ s?: o, s?: I, s: o }",
		"ts", &json_ts,
		"sequence", &sequence,
		"data", &json_data);
	if (ret)
		return ret;

	smp->flags = 0;
	smp->length = 0;

	if (json_ts) {
		ret = json_unpack_timestamps(json_ts, smp);
		if (ret)
			return ret;
	}

	if (!json_is_array(json_data))
		return -1;

	if (sequence >= 0) {
		smp->sequence = sequence;
		smp->flags |= SAMPLE_HAS_SEQUENCE;
	}

	json_array_foreach(json_data, i, json_value) {
		if (i >= smp->capacity)
			break;

		struct signal *sig = vlist_at_safe(smp->signals, i);
		if (!sig)
			return -1;

		enum signal_type fmt = json_detect_format(json_value);
		if (sig->type == SIGNAL_TYPE_AUTO) {
			debug(LOG_IO | 5, "Learned data type for index %zu: %s", i, signal_type_to_str(fmt));
			sig->type = fmt;
		}
		else if (sig->type != fmt) {
			error("Received invalid data type in JSON payload: Received %s, expected %s for signal %s (index %zu).",
				signal_type_to_str(fmt), signal_type_to_str(sig->type), sig->name, i);
			return -2;
		}

		ret = signal_data_parse_json(&smp->data[i], sig, json_value);
		if (ret)
			return -3;

		smp->length++;
	}

	if (smp->length > 0)
		smp->flags |= SAMPLE_HAS_DATA;

	return 0;
}

static int json_unpack_samples(struct io *io, json_t *json_smps, struct sample *smps[], unsigned cnt)
{
	int ret;
	json_t *json_smp;
	size_t i;

	if (!json_is_array(json_smps))
		return -1;

	json_array_foreach(json_smps, i, json_smp) {
		if (i >= cnt)
			break;

		ret = json_unpack_sample(io, json_smp, smps[i]);
		if (ret < 0)
			break;
	}

	return i;
}

int json_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	int ret;
	json_t *json;
	size_t wr;

	ret = json_pack_samples(io, &json, smps, cnt);
	if (ret < 0)
		return ret;

	wr = json_dumpb(json, buf, len, 0);

	json_decref(json);

	if (wbytes)
		*wbytes = wr;

	return ret;
}

int json_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	int ret;
	json_t *json;
	json_error_t err;

	json = json_loadb(buf, len, 0, &err);
	if (!json)
		return -1;

	ret = json_unpack_samples(io, json, smps, cnt);

	json_decref(json);

	if (ret < 0)
		return ret;

	if (rbytes)
		*rbytes = err.position;

	return ret;
}

int json_print(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret, i;
	json_t *json;

	FILE *f = io_stream_output(io);

	for (i = 0; i < cnt; i++) {
		ret = json_pack_sample(io, &json, smps[i]);
		if (ret)
			return ret;

		ret = json_dumpf(json, f, 0);
		fputc(io->delimiter, f);

		json_decref(json);

		if (ret)
			return ret;
	}

	return i;
}

int json_scan(struct io *io, struct sample *smps[], unsigned cnt)
{
	int i, ret;
	json_t *json;
	json_error_t err;

	FILE *f = io_stream_input(io);

	for (i = 0; i < cnt; i++) {
		if (feof(f))
			return -1;

skip:		json = json_loadf(f, JSON_DISABLE_EOF_CHECK, &err);
		if (!json)
			break;

		ret = json_unpack_sample(io, json, smps[i]);
		if (ret)
			goto skip;

		json_decref(json);
	}

	return i;
}

static struct plugin p = {
	.name = "json",
	.description = "Javascript Object Notation",
	.type = PLUGIN_TYPE_FORMAT,
	.format = {
		.scan	= json_scan,
		.print	= json_print,
		.sscan	= json_sscan,
		.sprint	= json_sprint,
		.size = 0,
		.delimiter = '\n',
		.flags =  IO_AUTO_DETECT_FORMAT |
		          SAMPLE_HAS_TS_ORIGIN | SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_DATA
	},
};

REGISTER_PLUGIN(&p);
