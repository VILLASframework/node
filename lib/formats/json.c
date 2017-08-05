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

size_t io_format_json_sprint(char *buf, size_t len, struct sample *smps[], size_t cnt, int flags)
{
	int i, ret;
	json_t *json;
	size_t wr, off = 0;

	for (i = 0; i < cnt; i++) {
		ret = io_format_json_pack(&json, smps[i], flags);
		if (ret)
			return ret;

		wr = json_dumpb(json, buf + off, len - off, 0);

		json_decref(json);

		if (wr > len)
			break;

		off += wr;
	}

	return i;
}

size_t io_format_json_sscan(char *buf, size_t len, struct sample *smps[], size_t cnt, int *flags)
{
	int i, ret;
	json_t *json;
	json_error_t err;
	size_t off = 0;

	for (i = 0; i < cnt; i++) {
		json = json_loadb(buf + off, len - off, JSON_DISABLE_EOF_CHECK, &err);
		if (!json)
			break;

		off += err.position;

		ret = io_format_json_unpack(json, smps[i], flags);

		json_decref(json);

		if (ret)
			break;
	}

	return i;
}

int io_format_json_fprint(FILE *f, struct sample *smps[], size_t cnt, int flags)
{
	int ret, i;
	json_t *json;

	for (i = 0; i < cnt; i++) {
		ret = io_format_json_pack(&json, smps[i], flags);
		if (ret)
			return ret;

		ret = json_dumpf(json, f, 0);
		fputc('\n', f);

		json_decref(json);
	}

	return i;
}

int io_format_json_fscan(FILE *f, struct sample *smps[], size_t cnt, int *flags)
{
	int i, ret;
	json_t *json;
	json_error_t err;

	for (i = 0; i < cnt; i++) {
skip:		json = json_loadf(f, JSON_DISABLE_EOF_CHECK, &err);
		if (!json)
			break;

		ret = io_format_json_unpack(json, smps[i], flags);
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
	.io = {
		.fscan	= io_format_json_fscan,
		.fprint	= io_format_json_fprint,
		.sscan	= io_format_json_sscan,
		.sprint	= io_format_json_sprint,
		.size = 0
	},
};

REGISTER_PLUGIN(&p);
