/** JSON serializtion for RESERVE project.
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
#include <villas/formats/json_reserve.h>

#define JSON_RESERVE_INTEGER_TARGET 1

static int json_reserve_pack_sample(struct io *io, json_t **j, struct sample *smp)
{
	json_error_t err;
	json_t *json_data, *json_name, *json_unit, *json_value;
	json_t *json_created = nullptr;
	json_t *json_sequence = nullptr;

	if (smp->flags & (int) SampleFlags::HAS_TS_ORIGIN)
		json_created = json_integer(time_to_double(&smp->ts.origin) * 1e3);

	if (smp->flags & (int) SampleFlags::HAS_SEQUENCE)
		json_sequence = json_integer(smp->sequence);

	json_data = json_array();

	for (unsigned i = 0; i < smp->length; i++) {
		struct signal *sig = (struct signal *) vlist_at_safe(smp->signals, i);
		if (!sig)
			return -1;

		if (sig->name)
			json_name = json_string(sig->name);
		else {
			char name[32];
			snprintf(name, 32, "signal%u", i);

			json_name = json_string(name);
		}

		if (sig->unit)
			json_unit = json_string(sig->unit);
		else
			json_unit = nullptr;

		json_value = json_pack_ex(&err, 0, "{ s: o, s: f }",
			"name", json_name,
			"value", smp->data[i].f
		);
		if (!json_value)
			continue;

		if (json_unit)
			json_object_set(json_value, "unit", json_unit);
		if (json_created)
			json_object_set(json_value, "created", json_created);
		if (json_sequence)
			json_object_set(json_value, "sequence", json_sequence);

		json_array_append(json_data, json_value);
	}

	if (json_created)
		json_decref(json_created);
	if (json_sequence)
		json_decref(json_sequence);

	*j = json_pack_ex(&err, 0, "{ s: o }",
		"measurements", json_data
	);
	if (*j == nullptr)
		return -1;
#if 0
#ifdef JSON_RESERVE_INTEGER_TARGET
	if (io->out.node) {
		char *endptr;
		char *id_str = strrchr(io->out.node->name, '_');
		if (!id_str)
			return -1;

		int id = strtoul(id_str+1, &endptr, 10);
		if (endptr[0] != 0)
			return -1;

		json_object_set_new(*j, "target", json_integer(id));
	}
#else
	if (io->out.node)
		json_object_set_new(*j, "target", json_string(io->out.node->name));
#endif
#endif

	return 0;
}

static int json_reserve_unpack_sample(struct io *io, json_t *json_smp, struct sample *smp)
{
	int ret, idx;
	double created = -1;
	json_error_t err;
	json_t *json_value, *json_data = nullptr;
	json_t *json_origin = nullptr, *json_target = nullptr;
	size_t i;

	ret = json_unpack_ex(json_smp, &err, 0, "{ s?: o, s?: o, s?: o, s?: o }",
		"origin", &json_origin,
		"target", &json_target,
		"measurements", &json_data,
		"setpoints", &json_data
	);
	if (ret)
		return -1;

#if 0
#ifdef JSON_RESERVE_INTEGER_TARGET
	if (json_target && io->in.node) {
		if (!json_is_integer(json_target))
			return -1;

		char *endptr;
		char *id_str = strrchr(io->in.node->name, '_');
		if (!id_str)
			return -1;

		int id = strtoul(id_str+1, &endptr, 10);
		if (endptr[0] != 0)
			return -1;

		if (id != json_integer_value(json_target))
			return 0;
	}
#else
	if (json_target && io->in.node) {
		const char *target = json_string_value(json_target);
		if (!target)
			return -1;

		if (strcmp(target, io->in.node->name))
			return 0;
	}
#endif
#endif
	if (!json_data || !json_is_array(json_data))
		return -1;

	smp->flags = 0;
	smp->length = 0;

	json_array_foreach(json_data, i, json_value) {
		const char *name, *unit = nullptr;
		double value;

		ret = json_unpack_ex(json_value, &err, 0, "{ s: s, s?: s, s: F, s?: F }",
			"name", &name,
			"unit", &unit,
			"value", &value,
			"created", &created
		);
		if (ret)
			return -1;

		struct signal *sig;

		sig = vlist_lookup_name<struct signal>(io->signals, name);
		if (sig) {
			if (!sig->enabled)
				continue;

			idx = vlist_index(io->signals, sig);
		}
		else {
			ret = sscanf(name, "signal_%d", &idx);
			if (ret != 1)
				continue;
		}

		if (idx < 0)
			return -1;

		if (idx < (int) smp->capacity) {
			smp->data[idx].f = value;

			if (idx >= (int) smp->length)
				smp->length = idx + 1;
		}
	}

	if (smp->length > 0)
		smp->flags |= (int) SampleFlags::HAS_DATA;

	if (created > 0) {
		smp->ts.origin = time_from_double(created * 1e-3);
		smp->flags |= (int) SampleFlags::HAS_TS_ORIGIN;
	}

	return smp->length > 0 ? 1 : 0;
}

/*
 * Note: The following functions are the same as io/json.c !!!
 */

int json_reserve_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	int ret;
	json_t *json;
	size_t wr;

	assert(cnt == 1);

	ret = json_reserve_pack_sample(io, &json, smps[0]);
	if (ret < 0)
		return ret;

	wr = json_dumpb(json, buf, len, 0);

	json_decref(json);

	if (wbytes)
		*wbytes = wr;

	return ret;
}

int json_reserve_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	int ret;
	json_t *json;
	json_error_t err;

	assert(cnt == 1);

	json = json_loadb(buf, len, 0, &err);
	if (!json)
		return -1;

	ret = json_reserve_unpack_sample(io, json, smps[0]);

	json_decref(json);

	if (ret < 0)
		return ret;

	if (rbytes)
		*rbytes = err.position;

	return ret;
}

int json_reserve_print(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;
	unsigned i;
	json_t *json;

	FILE *f = io_stream_output(io);

	for (i = 0; i < cnt; i++) {
		ret = json_reserve_pack_sample(io, &json, smps[i]);
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

int json_reserve_scan(struct io *io, struct sample *smps[], unsigned cnt)
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

		ret = json_reserve_unpack_sample(io, json, smps[i]);

		json_decref(json);

		if (ret < 0)
			goto skip;
	}

	return i;
}

static struct plugin p;

__attribute__((constructor(110))) static void UNIQUE(__ctor)() {
	p.name = "json.reserve";
	p.description = "RESERVE JSON format";
	p.type = PluginType::FORMAT;
	p.format.print	= json_reserve_print;
	p.format.scan	= json_reserve_scan;
	p.format.sprint	= json_reserve_sprint;
	p.format.sscan	= json_reserve_sscan;
	p.format.size = 0;

	vlist_init_and_push(&plugins, &p);
}

__attribute__((destructor(110))) static void UNIQUE(__dtor)() {
	vlist_remove_all(&plugins, &p);
}
