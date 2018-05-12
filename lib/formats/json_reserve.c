/** JSON serializtion for RESERVE project.
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

#include <villas/plugin.h>
#include <villas/sample.h>
#include <villas/node.h>
#include <villas/signal.h>
#include <villas/compat.h>
#include <villas/timing.h>
#include <villas/io.h>
#include <villas/formats/json.h>

static int json_reserve_pack_sample(struct io *io, json_t **j, struct sample *smp)
{
	json_error_t err;
	json_t *json_data, *json_name, *json_unit, *json_value;
	json_t *json_created = NULL, *json_sequence = NULL;
	struct signal *sig;

	if (smp->flags & SAMPLE_HAS_ORIGIN)
		json_created = json_real(time_to_double(&smp->ts.origin));

	if (smp->flags & SAMPLE_HAS_SEQUENCE)
		json_sequence = json_integer(smp->sequence);

	json_data = json_array();

	for (int i = 0; i < smp->length; i++) {
		if (io->output.signals)
			sig = (struct signal *) list_at_safe(io->output.signals, i);
		else
			sig = NULL;

		if (sig) {
			json_name = json_string(sig->name);
			json_unit = json_string(sig->unit);
		}
		else {
			char name[32];
			snprintf(name, 32, "signal_%d", i);

			json_name = json_string(name);
			json_unit = NULL;
		}

		json_value = json_pack_ex(&err, 0, "{ s: o, s: o*, s: f, s: o*, s: o* }",
			"name", json_name,
			"unit", json_unit,
			"value", smp->data[i].f,
			"created", json_created,
			"sequence", json_sequence
		);
		if (!json_value)
			continue;

		json_array_append(json_data, json_value);
	}

	char *origin, *target;
	origin = smp->source
		? smp->source->name
		: NULL;

	target = smp->destination
		? smp->destination->name
		: NULL;

	*j = json_pack_ex(&err, 0, "{ s: s*, s: s*, s: o }",
		"origin", origin,
		"target", target,
		"measurements", json_data
	);
	if (*j == NULL)
		return -1;

	return 0;
}

static int json_reserve_unpack_sample(struct io *io, json_t *json_smp, struct sample *smp)
{
	int ret, idx;
	double created = -1;
	json_error_t err;
	json_t *json_value, *json_data = NULL;
	size_t i;

	const char *origin;
	const char *target;

	ret = json_unpack_ex(json_smp, &err, 0, "{ s?: s, s?: s, s?: o, s?: o }",
		"origin", &origin,
		"target", &target,
		"measurements", &json_data,
		"setpoints", &json_data
	);
	if (ret)
		return -1;

	if (!json_data || !json_is_array(json_data))
		return -1;

	smp->flags = 0;
	smp->length = 0;

	json_array_foreach(json_data, i, json_value) {
		const char *name, *unit = NULL;
		double value;

		ret = json_unpack_ex(json_value, &err, 0, "{ s: s, s?: s, s: f, s?: f }",
			"name", &name,
			"unit", &unit,
			"value", &value,
			"created", &created
		);
		if (ret)
			return -1;

		void *s = list_lookup(io->input.signals, name);
		if (s)
			idx = list_index(io->input.signals, s);
		else {
			ret = sscanf(name, "signal_%d", &idx);
			if (ret != 1)
				continue;
		}

		if (idx < smp->capacity) {
			smp->data[idx].f = value;

			if (idx >= smp->length)
				smp->length = idx;
		}
	}

	if (smp->length > 0)
		smp->flags |= SAMPLE_HAS_VALUES;

	if (created > 0) {
		smp->ts.origin = time_from_double(created);
		smp->flags |= SAMPLE_HAS_ORIGIN;
	}

	return 0;
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

int json_reserve_sscan(struct io *io, char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
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

	return 1;
}

int json_reserve_print(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret, i;
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
	int i, ret;
	json_t *json;
	json_error_t err;

	FILE *f = io_stream_input(io);

	for (i = 0; i < cnt; i++) {
skip:		json = json_loadf(f, JSON_DISABLE_EOF_CHECK, &err);
		if (!json)
			break;

		ret = json_reserve_unpack_sample(io, json, smps[i]);
		if (ret)
			goto skip;

		json_decref(json);
	}

	return i;
}

static struct plugin p = {
	.name = "json.reserve",
	.description = "RESERVE JSON format",
	.type = PLUGIN_TYPE_FORMAT,
	.io = {
		.scan	= json_reserve_scan,
		.print	= json_reserve_print,
		.sscan	= json_reserve_sscan,
		.sprint	= json_reserve_sprint,
		.size = 0
	},
};

REGISTER_PLUGIN(&p);
