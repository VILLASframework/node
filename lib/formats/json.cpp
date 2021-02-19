/** JSON serializtion of sample data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/compat.hpp>
#include <villas/signal.h>
#include <villas/io.h>
#include <villas/formats/json.h>

static enum SignalType json_detect_format(json_t *val)
{
	int type = json_typeof(val);

	switch (type) {
		case JSON_REAL:
			return SignalType::FLOAT;

		case JSON_INTEGER:
			return SignalType::INTEGER;

		case JSON_TRUE:
		case JSON_FALSE:
			return SignalType::BOOLEAN;

		case JSON_OBJECT:
			return SignalType::COMPLEX; /* must be a complex number */

		default:
			return SignalType::INVALID;
	}
}

static json_t * json_pack_timestamps(struct io *io, struct sample *smp)
{
	json_t *json_ts = json_object();

#ifdef __arm__ // TODO: check
	const char *fmt = "[ i, i ]";
#else
	const char *fmt = "[ I, I ]";
#endif

	if (io->flags & (int) SampleFlags::HAS_TS_ORIGIN) {
		if (smp->flags & (int) SampleFlags::HAS_TS_ORIGIN)
			json_object_set(json_ts, "origin", json_pack(fmt, smp->ts.origin.tv_sec, smp->ts.origin.tv_nsec));
	}

	if (io->flags & (int) SampleFlags::HAS_TS_RECEIVED) {
		if (smp->flags & (int) SampleFlags::HAS_TS_RECEIVED)
			json_object_set(json_ts, "received", json_pack(fmt, smp->ts.received.tv_sec, smp->ts.received.tv_nsec));
	}

	return json_ts;
}

static int json_unpack_timestamps(json_t *json_ts, struct sample *smp)
{
	int ret;
	json_error_t err;
	json_t *json_ts_origin = nullptr;
	json_t *json_ts_received = nullptr;

	json_unpack_ex(json_ts, &err, 0, "{ s?: o, s?: o }",
		"origin",   &json_ts_origin,
		"received", &json_ts_received
	);

	if (json_ts_origin) {
		ret = json_unpack_ex(json_ts_origin, &err, 0, "[ I, I ]", &smp->ts.origin.tv_sec, &smp->ts.origin.tv_nsec);
		if (ret)
			return ret;

		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;
	}

	if (json_ts_received) {
		ret = json_unpack_ex(json_ts_received, &err, 0, "[ I, I ]", &smp->ts.received.tv_sec, &smp->ts.received.tv_nsec);
		if (ret)
			return ret;

		smp->flags |= (int) SampleFlags::HAS_TS_RECEIVED;
	}

	return 0;
}

static int json_pack_sample(struct io *io, json_t **j, struct sample *smp)
{
	json_t *json_smp;
	json_error_t err;

	json_smp = json_pack_ex(&err, 0, "{ s: o }", "ts", json_pack_timestamps(io, smp));

	if (io->flags & (int) SampleFlags::HAS_SEQUENCE) {
		if (smp->flags & (int) SampleFlags::HAS_SEQUENCE) {
			json_t *json_sequence = json_integer(smp->sequence);

			json_object_set(json_smp, "sequence", json_sequence);
		}
	}

	if (io->flags & (int) SampleFlags::HAS_DATA) {
		json_t *json_data = json_array();

		for (unsigned i = 0; i < smp->length; i++) {
			struct signal *sig = (struct signal *) vlist_at_safe(smp->signals, i);
			if (!sig)
				return -1;

			json_t *json_value = signal_data_to_json(&smp->data[i], sig->type);

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

	for (unsigned i = 0; i < cnt; i++) {
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
	json_t *json_data, *json_value, *json_ts = nullptr;
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
		smp->flags |= (int) SampleFlags::HAS_SEQUENCE;
	}

	json_array_foreach(json_data, i, json_value) {
		if (i >= smp->capacity)
			break;

		struct signal *sig = (struct signal *) vlist_at_safe(smp->signals, i);
		if (!sig)
			return -1;

		enum SignalType fmt = json_detect_format(json_value);
		if (sig->type != fmt) {
			error("Received invalid data type in JSON payload: Received %s, expected %s for signal %s (index %zu).",
				signal_type_to_str(fmt), signal_type_to_str(sig->type), sig->name, i);
			return -2;
		}

		ret = signal_data_parse_json(&smp->data[i], sig->type, json_value);
		if (ret)
			return -3;

		smp->length++;
	}

	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

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
	int ret;
	unsigned i;
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
	int ret;
	unsigned i;
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

static struct plugin p;

__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	p.name = "json";
	p.description = "Javascript Object Notation";
	p.type = PluginType::FORMAT;
	p.format.print = json_print;
	p.format.scan = json_scan;
	p.format.sprint = json_sprint;
	p.format.sscan = json_sscan;
	p.format.size = 0;
	p.format.flags = (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA;
	p.format.delimiter = '\n';

	vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
	vlist_remove_all(&plugins, &p);
}
