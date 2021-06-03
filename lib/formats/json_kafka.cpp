/** JSON serializtion for Kafka schema/payloads.
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

#include <cstring>

#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/signal.h>
#include <villas/compat.hpp>
#include <villas/timing.h>
#include <villas/io.h>
#include <villas/formats/json_kafka.h>

static const char * json_kafka_type_villas_to_kafka(enum SignalType vt)
{
	switch (vt) {
		case SignalType::FLOAT:
			return "double";

		case SignalType::INTEGER:
			return "int64";

		case SignalType::BOOLEAN:
			return "?";

		case SignalType::COMPLEX:
			return "?";

		default:
		case SignalType::INVALID:
			return "unknown";
	}
}

static int json_kafka_pack_sample(struct io *io, json_t **json_smp, struct sample *smp)
{
	json_t *json_root, *json_schema, *json_payload, *json_fields, *json_field, *json_value;

	json_fields = json_array();
	json_payload = json_object();

	/* Include sample timestamp */
	if (smp->flags & (int) SampleFlags::HAS_TS_ORIGIN) {
		json_field = json_pack("{ s: s, s: b, s: s }",
			"type", "int64",
			"optional", false,
			"field", "timestamp"
		);

		uint64_t ts_origin_ms = smp->ts.origin.tv_sec * 1e3 + smp->ts.origin.tv_nsec / 1e6;
		json_array_append(json_fields, json_field);
		json_object_set(json_payload, "timestamp", json_integer(ts_origin_ms));
	}

	/* Include sample sequence no */
	if (smp->flags & (int) SampleFlags::HAS_SEQUENCE) {
		json_field = json_pack("{ s: s, s: b, s: s }",
			"type", "int64",
			"optional", false,
			"field", "sequence"
		);

		json_array_append(json_fields, json_field);
		json_object_set(json_payload, "sequence", json_integer(smp->sequence));
	}

	/* Include sample data */
	for (size_t i = 0; i < MIN(smp->length, vlist_length(smp->signals)); i++) {
		struct signal *sig = (struct signal *) vlist_at(smp->signals, i);
		union signal_data *data = &smp->data[i];

		json_field = json_pack("{ s: s, s: b, s: s }",
			"type", json_kafka_type_villas_to_kafka(sig->type),
			"optional", false,
			"field", sig->name
		);

		json_value = signal_data_to_json(data, sig->type);

		json_array_append(json_fields, json_field);
		json_object_set(json_payload, sig->name, json_value);
	}

	json_schema = json_pack("{ s: s, s: s, s: o }",
		"type", "struct",
		"name", "villas-node.Value",
		"fields", json_fields
	);

	json_root = json_pack("{ s: o, s: o }",
		"schema", json_schema,
		"payload", json_payload
	);

	*json_smp = json_root;

	return 0;
}

static int json_kafka_unpack_sample(struct io *io, json_t *json_smp, struct sample *smp)
{
	json_t *json_payload, *json_value;

	json_payload = json_object_get(json_smp, "payload");
	if (!json_payload)
		return -1;

	smp->length = 0;
	smp->flags = 0;
	smp->signals = io->signals;

	/* Get timestamp */
	json_value = json_object_get(json_payload, "timestamp");
	if (json_value) {
		uint64_t ts_origin_ms = json_integer_value(json_value);
		smp->ts.origin = time_from_double(ts_origin_ms / 1e3);

		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;
	}

	/* Get sequence no */
	json_value = json_object_get(json_payload, "sequence");
	if (json_value) {
		smp->sequence = json_integer_value(json_value);
	
		smp->flags |= (int) SampleFlags::HAS_SEQUENCE;
	}

	/* Get sample signals */
	for (size_t i = 0; i < vlist_length(io->signals); i++) {
		struct signal *sig = (struct signal *) vlist_at(io->signals, i);
		
		json_value = json_object_get(json_payload, sig->name);
		if (!json_value)
			continue;

		signal_data_parse_json(&smp->data[i], sig->type, json_value);
		smp->length++;
	}

	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

	return 1;
}

/*
 * Note: The following functions are the same as io/json.c !!!
 */

int json_kafka_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	int ret;
	json_t *json;
	size_t wr;

	assert(cnt == 1);

	ret = json_kafka_pack_sample(io, &json, smps[0]);
	if (ret < 0)
		return ret;

	wr = json_dumpb(json, buf, len, 0);

	json_decref(json);

	if (wbytes)
		*wbytes = wr;

	return ret;
}

int json_kafka_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	int ret;
	json_t *json;
	json_error_t err;

	assert(cnt == 1);

	json = json_loadb(buf, len, 0, &err);
	if (!json)
		return -1;

	ret = json_kafka_unpack_sample(io, json, smps[0]);

	json_decref(json);

	if (ret < 0)
		return ret;

	if (rbytes)
		*rbytes = err.position;

	return ret;
}

int json_kafka_print(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;
	unsigned i;
	json_t *json;

	FILE *f = io_stream_output(io);

	for (i = 0; i < cnt; i++) {
		ret = json_kafka_pack_sample(io, &json, smps[i]);
		if (ret)
			return ret;

		ret = json_dumpf(json, f, 0);
		fputc('\n', f);

		json_decref(json);

		if (ret)
			return ret;
	}

	return i;
}

int json_kafka_scan(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;
	unsigned i;
	json_t *json;
	json_error_t err;

	FILE *f = io_stream_input(io);

	for (i = 0; i < cnt; i++) {
skip:		json = json_loadf(f, JSON_DISABLE_EOF_CHECK, &err);
		if (!json)
			break;

		ret = json_kafka_unpack_sample(io, json, smps[i]);

		json_decref(json);

		if (ret < 0)
			goto skip;
	}

	return i;
}

static struct plugin p;

__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	p.name = "json.kafka";
	p.description = "JSON Kafka schema/payload messages";
	p.type = PluginType::FORMAT;
	p.format.print	= json_kafka_print;
	p.format.scan	= json_kafka_scan;
	p.format.sprint	= json_kafka_sprint;
	p.format.sscan	= json_kafka_sscan;
	p.format.size = 0;

	vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
	vlist_remove_all(&plugins, &p);
}
