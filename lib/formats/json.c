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
#include "formats/json.h"

int io_format_json_pack(json_t **j, struct sample *s, int flags)
{
	json_error_t err;
	json_t *json_data = json_array();

	for (int i = 0; i < s->length; i++) {
		json_t *json_value = sample_get_data_format(s, i)
					? json_integer(s->data[i].i)
					: json_real(s->data[i].f);

		json_array_append(json_data, json_value);
	}

	*j = json_pack_ex(&err, 0, "{ s: { s: [ I, I ], s: [ I, I ], s: [ I, I ] }, s: I, s: o }",
		"ts",
			"origin", s->ts.origin.tv_sec, s->ts.origin.tv_nsec,
			"received", s->ts.received.tv_sec, s->ts.received.tv_nsec,
			"sent", s->ts.sent.tv_sec, s->ts.sent.tv_nsec,
		"sequence", s->sequence,
		"data", json_data);

	if (!*j)
		return -1;

	return 0;
}

int io_format_json_unpack(json_t *j, struct sample *s, int *flags)
{
	int ret, i;
	json_t *json_data, *json_value;

	ret = json_unpack(j, "{ s: { s: [ I, I ], s: [ I, I ], s: [ I, I ] }, s: I, s: o }",
		"ts",
			"origin", &s->ts.origin.tv_sec, &s->ts.origin.tv_nsec,
			"received", &s->ts.received.tv_sec, &s->ts.received.tv_nsec,
			"sent", &s->ts.sent.tv_sec, &s->ts.sent.tv_nsec,
		"sequence", &s->sequence,
		"data", &json_data);

	if (ret)
		return ret;

	s->length = 0;

	json_array_foreach(json_data, i, json_value) {
		switch (json_typeof(json_value)) {
			case JSON_REAL:
				s->data[i].f = json_real_value(json_value);
				sample_set_data_format(s, i, SAMPLE_DATA_FORMAT_FLOAT);
				break;

			case JSON_INTEGER:
				s->data[i].f = json_integer_value(json_value);
				sample_set_data_format(s, i, SAMPLE_DATA_FORMAT_INT);
				break;

			default:
				return -1;
		}

		s->length++;
	}

	return 0;
}

int io_format_json_fprint(AFILE *f, struct sample *s, int flags)
{
	int ret;
	json_t *json;

	ret = io_format_json_pack(&json, s, flags);
	if (ret)
		return ret;

	ret = json_dumpf(json, f->file, 0);

	json_decref(json);

	return ret;
}

int io_format_json_fscan(AFILE *f, struct sample *s, int *flags)
{
	int ret;
	json_t *json;
	json_error_t err;

	json = json_loadf(f->file, JSON_DISABLE_EOF_CHECK, &err);
	if (!json)
		return -1;

	ret = io_format_json_unpack(json, s, flags);

	json_decref(json);

	return ret;
}

int io_format_json_print(struct io *io, struct sample *smp, int flags)
{
	return io_format_json_fprint(io->file, smp, flags);
}

int io_format_json_scan(struct io *io, struct sample *smp, int *flags)
{
	return io_format_json_fscan(io->file, smp, flags);
}

static struct plugin p = {
	.name = "json",
	.description = "Javascript Object Notation",
	.type = PLUGIN_TYPE_FORMAT,
	.io = {
		.print	= io_format_json_print,
		.scan	= io_format_json_scan,
		.size = 0
	},
};

REGISTER_PLUGIN(&p);
