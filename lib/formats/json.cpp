/** JSON serializtion of sample data.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/sample.hpp>
#include <villas/compat.hpp>
#include <villas/signal.hpp>
#include <villas/formats/json.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;

enum SignalType JsonFormat::detect(const json_t *val)
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

json_t * JsonFormat::packTimestamps(const struct Sample *smp)
{
	json_t *json_ts = json_object();

#ifdef __arm__ // TODO: check
	const char *fmt = "[ i, i ]";
#else
	const char *fmt = "[ I, I ]";
#endif

	if (flags & (int) SampleFlags::HAS_TS_ORIGIN) {
		if (smp->flags & (int) SampleFlags::HAS_TS_ORIGIN)
			json_object_set(json_ts, "origin", json_pack(fmt, smp->ts.origin.tv_sec, smp->ts.origin.tv_nsec));
	}

	if (flags & (int) SampleFlags::HAS_TS_RECEIVED) {
		if (smp->flags & (int) SampleFlags::HAS_TS_RECEIVED)
			json_object_set(json_ts, "received", json_pack(fmt, smp->ts.received.tv_sec, smp->ts.received.tv_nsec));
	}

	return json_ts;
}

int JsonFormat::unpackTimestamps(json_t *json_ts, struct Sample *smp)
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

int JsonFormat::packSample(json_t **json_smp, const struct Sample *smp)
{
	json_t *json_root;
	json_error_t err;

	json_root = json_pack_ex(&err, 0, "{ s: o }", "ts", packTimestamps(smp));

	if (flags & (int) SampleFlags::HAS_SEQUENCE) {
		if (smp->flags & (int) SampleFlags::HAS_SEQUENCE) {
			json_t *json_sequence = json_integer(smp->sequence);

			json_object_set_new(json_root, "sequence", json_sequence);
		}
	}

	if (flags & (int) SampleFlags::HAS_DATA) {
		json_t *json_data = json_array();

		for (unsigned i = 0; i < smp->length; i++) {
			auto sig = smp->signals->getByIndex(i);
			if (!sig)
				return -1;

			json_t *json_value = smp->data[i].toJson(sig->type);

			json_array_append_new(json_data, json_value);
		}

		json_object_set_new(json_root, "data", json_data);
	}

	*json_smp = json_root;

	return 0;
}

int JsonFormat::packSamples(json_t **json_smps, const struct Sample * const smps[], unsigned cnt)
{
	int ret;
	json_t *json_root = json_array();

	for (unsigned i = 0; i < cnt; i++) {
		json_t *json_smp;

		ret = packSample(&json_smp, smps[i]);
		if (ret)
			break;

		json_array_append_new(json_root, json_smp);
	}

	*json_smps = json_root;

	return cnt;
}

int JsonFormat::unpackSample(json_t *json_smp, struct Sample *smp)
{
	int ret;
	json_error_t err;
	json_t *json_data, *json_value, *json_ts = nullptr;
	size_t i;
	int64_t sequence = -1;

	smp->signals = signals;

	ret = json_unpack_ex(json_smp, &err, 0, "{ s?: o, s?: I, s: o }",
		"ts", &json_ts,
		"sequence", &sequence,
		"data", &json_data);
	if (ret)
		return ret;

	smp->flags = 0;
	smp->length = 0;

	if (json_ts) {
		ret = unpackTimestamps(json_ts, smp);
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

		auto sig = smp->signals->getByIndex(i);
		if (!sig)
			return -1;

		enum SignalType fmt = detect(json_value);
		if (sig->type != fmt)
			throw RuntimeError("Received invalid data type in JSON payload: Received {}, expected {} for signal {} (index {}).",
				signalTypeToString(fmt), signalTypeToString(sig->type), sig->name, i);

		ret = smp->data[i].parseJson(sig->type, json_value);
		if (ret)
			return -3;

		smp->length++;
	}

	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

	return 0;
}

int JsonFormat::unpackSamples(json_t *json_smps, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	json_t *json_smp;
	size_t i;

	if (!json_is_array(json_smps))
		return -1;

	json_array_foreach(json_smps, i, json_smp) {
		if (i >= cnt)
			break;

		ret = unpackSample(json_smp, smps[i]);
		if (ret < 0)
			break;
	}

	return i;
}

int JsonFormat::sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt)
{
	int ret;
	json_t *json;
	size_t wr;

	ret = packSamples(&json, smps, cnt);
	if (ret < 0)
		return ret;

	wr = json_dumpb(json, buf, len, dump_flags);

	json_decref(json);

	if (wbytes)
		*wbytes = wr;

	return ret;
}

int JsonFormat::sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	json_t *json;
	json_error_t err;

	json = json_loadb(buf, len, 0, &err);
	if (!json)
		return -1;

	ret = unpackSamples(json, smps, cnt);

	json_decref(json);

	if (ret < 0)
		return ret;

	if (rbytes)
		*rbytes = err.position;

	return ret;
}

int JsonFormat::print(FILE *f, const struct Sample * const smps[], unsigned cnt)
{
	int ret;
	unsigned i;
	json_t *json;

	for (i = 0; i < cnt; i++) {
		ret = packSample(&json, smps[i]);
		if (ret)
			return ret;

		ret = json_dumpf(json, f, dump_flags);
		fputc('\n', f);

		json_decref(json);

		if (ret)
			return ret;
	}

	return i;
}

int JsonFormat::scan(FILE *f, struct Sample * const smps[], unsigned cnt)
{
	int ret;
	unsigned i;
	json_t *json;
	json_error_t err;

	for (i = 0; i < cnt; i++) {
		if (feof(f))
			return -1;

skip:		json = json_loadf(f, JSON_DISABLE_EOF_CHECK, &err);
		if (!json)
			break;

		ret = unpackSample(json, smps[i]);
		if (ret)
			goto skip;

		json_decref(json);
	}

	return i;
}

void JsonFormat::parse(json_t *json)
{
	int ret;
	json_error_t err;

	int indent = 0;
	int compact = 0;
	int ensure_ascii = 0;
	int escape_slash = 0;
	int sort_keys = 0;

	ret = json_unpack_ex(json, &err, 0, "{ s?: i, s?: b, s?: b, s?: b, s?: b }",
		"indent", &indent,
		"compact", &compact,
		"ensure_ascii", &ensure_ascii,
		"escape_slash", &escape_slash,
		"sort_keys", &sort_keys

	);
	if (ret)
		throw ConfigError(json, err, "node-config-format-json", "Failed to parse format configuration");

	if (indent > JSON_MAX_INDENT)
		throw ConfigError(json, "node-config-format-json-indent", "The maximum indentation level is {}", JSON_MAX_INDENT);

	dump_flags = 0;

	if (indent)
		dump_flags |= JSON_INDENT(indent);

	if (compact)
		dump_flags |= JSON_COMPACT;

	if (ensure_ascii)
		dump_flags |= JSON_ENSURE_ASCII;

	if (escape_slash)
		dump_flags |= JSON_ESCAPE_SLASH;

	if (sort_keys)
		dump_flags |= JSON_SORT_KEYS;

	Format::parse(json);

	if (real_precision)
		dump_flags |= JSON_REAL_PRECISION(real_precision);
}

static char n[] = "json";
static char d[] = "Javascript Object Notation";
static FormatPlugin<JsonFormat, n, d, (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA> p;
