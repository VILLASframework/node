/** JSON serializtion for Kafka schema/payloads.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/timing.hpp>
#include <villas/utils.hpp>
#include <villas/formats/json_kafka.hpp>
#include <villas/exceptions.hpp>

using namespace villas;
using namespace villas::node;

const char * JsonKafkaFormat::villasToKafkaType(enum SignalType vt)
{
	switch (vt) {
		case SignalType::FLOAT:
			return "double";

		case SignalType::INTEGER:
			return "int64";

		case SignalType::BOOLEAN:
			return "boolean";

		default:
		case SignalType::COMPLEX:
		case SignalType::INVALID:
			return "unknown";
	}
}

int JsonKafkaFormat::packSample(json_t **json_smp, const struct Sample *smp)
{
	json_t *json_payload, *json_fields, *json_field, *json_value;

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
		json_array_append_new(json_fields, json_field);
		json_object_set_new(json_payload, "timestamp", json_integer(ts_origin_ms));
	}

	/* Include sample sequence no */
	if (smp->flags & (int) SampleFlags::HAS_SEQUENCE) {
		json_field = json_pack("{ s: s, s: b, s: s }",
			"type", "int64",
			"optional", false,
			"field", "sequence"
		);

		json_array_append_new(json_fields, json_field);
		json_object_set_new(json_payload, "sequence", json_integer(smp->sequence));
	}

	/* Include sample data */
	for (size_t i = 0; i < MIN(smp->length, smp->signals->size()); i++) {
		const auto sig = smp->signals->getByIndex(i);
		const auto *data = &smp->data[i];

		json_field = json_pack("{ s: s, s: b, s: s }",
			"type", villasToKafkaType(sig->type),
			"optional", false,
			"field", sig->name
		);

		json_value = data->toJson(sig->type);

		json_array_append_new(json_fields, json_field);
		json_object_set_new(json_payload, sig->name.c_str(), json_value);
	}

	json_object_set_new(json_schema, "fields", json_fields);

	json_incref(json_schema);
	*json_smp = json_pack("{ s: o, s: o }",
		"schema", json_schema,
		"payload", json_payload
	);
	if (*json_smp == nullptr)
		return -1;

	return 0;
}

int JsonKafkaFormat::unpackSample(json_t *json_smp, struct Sample *smp)
{
	json_t *json_payload, *json_value;

	json_payload = json_object_get(json_smp, "payload");
	if (!json_payload)
		return -1;

	smp->length = 0;
	smp->flags = 0;
	smp->signals = signals;

	/* Unpack timestamp */
	json_value = json_object_get(json_payload, "timestamp");
	if (json_value) {
		uint64_t ts_origin_ms = json_integer_value(json_value);
		smp->ts.origin = time_from_double(ts_origin_ms / 1e3);

		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;
	}

	/* Unpack sequence no */
	json_value = json_object_get(json_payload, "sequence");
	if (json_value) {
		smp->sequence = json_integer_value(json_value);

		smp->flags |= (int) SampleFlags::HAS_SEQUENCE;
	}

	/* Unpack signal data */
	for (size_t i = 0; i < signals->size(); i++) {
		auto sig = signals->getByIndex(i);

		json_value = json_object_get(json_payload, sig->name.c_str());
		if (!json_value)
			continue;

		smp->data[i].parseJson(sig->type, json_value);
		smp->length++;
	}

	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

	return 0;
}

void JsonKafkaFormat::parse(json_t *json)
{
	int ret;

	json_error_t err;
	json_t *json_schema_tmp = nullptr;

	ret = json_unpack_ex(json, &err, 0, "{ s?: o }",
		"schema", &json_schema_tmp
	);
	if (ret)
		throw ConfigError(json, err, "node-config-format-json-kafka", "Failed to parse format configuration");

	if (json_schema_tmp) {
		if (!json_is_object(json_schema_tmp))
			throw ConfigError(json, "node-config-format-json-kafka-schema", "Kafka schema must be configured as a dictionary");

		if (json_schema)
			json_decref(json_schema);

		json_schema = json_schema_tmp;
	}

	JsonFormat::parse(json);
}

JsonKafkaFormat::JsonKafkaFormat(int fl) :
	JsonFormat(fl)
{
	json_schema = json_pack("{ s: s, s: s }",
		"type", "struct",
		"name", "villas-node.Value"
	);
}

static char n[] = "json.kafka";
static char d[] = "JSON Kafka schema/payload messages";
static FormatPlugin<JsonKafkaFormat, n, d, (int) SampleFlags::HAS_TS_ORIGIN | (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA> p;
