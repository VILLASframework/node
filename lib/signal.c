/** Signal meta data.
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

#include <string.h>
#include <inttypes.h>
#include <villas/signal.h>
#include <villas/list.h>
#include <villas/utils.h>
#include <villas/node.h>
#include <villas/mapping.h>

int signal_init(struct signal *s)
{
	s->enabled = true;

	s->name = NULL;
	s->unit = NULL;
	s->type = SIGNAL_TYPE_AUTO;

	s->refcnt = ATOMIC_VAR_INIT(1);

	return 0;
}

int signal_init_from_mapping(struct signal *s, const struct mapping_entry *me, unsigned index)
{
	int ret;

	ret = signal_init(s);
	if (ret)
		return ret;

	ret = mapping_to_str(me, index, &s->name);
	if (ret)
		return ret;

	switch (me->type) {
		case MAPPING_TYPE_STATS:
			switch (me->stats.type) {
				case STATS_TYPE_TOTAL:
					s->type = SIGNAL_TYPE_INTEGER;
					break;

				case STATS_TYPE_LAST:
				case STATS_TYPE_LOWEST:
				case STATS_TYPE_HIGHEST:
				case STATS_TYPE_MEAN:
				case STATS_TYPE_VAR:
				case STATS_TYPE_STDDEV:
					s->type = SIGNAL_TYPE_FLOAT;
					break;
			}
			break;

		case MAPPING_TYPE_HEADER:
			switch (me->header.type) {
				case MAPPING_HEADER_TYPE_LENGTH:
				case MAPPING_HEADER_TYPE_SEQUENCE:
					s->type = SIGNAL_TYPE_INTEGER;
					break;
			}
			break;

		case MAPPING_TYPE_TIMESTAMP:
			s->type = SIGNAL_TYPE_INTEGER;
			break;

		case MAPPING_TYPE_DATA:
			*s = *me->data.signal;
			break;
	}

	return 0;
}

int signal_destroy(struct signal *s)
{
	if (s->name)
		free(s->name);

	if (s->unit)
		free(s->unit);

	return 0;
}

struct signal * signal_create(const char *name, const char *unit, enum signal_type fmt)
{
	int ret;
	struct signal *sig;

	sig = alloc(sizeof(struct signal));
	if (!sig)
		return NULL;

	ret = signal_init(sig);
	if (ret)
		return NULL;

	if (name)
		sig->name = strdup(name);

	if (unit)
		sig->unit = strdup(unit);

	sig->type = fmt;

	return sig;
}

int signal_free(struct signal *s)
{
	int ret;

	ret = signal_destroy(s);
	if (ret)
		 return ret;

	free(s);

	return 0;
}

int signal_incref(struct signal *s)
{
	return atomic_fetch_add(&s->refcnt, 1) + 1;
}

int signal_decref(struct signal *s)
{
	int prev = atomic_fetch_sub(&s->refcnt, 1);

	/* Did we had the last reference? */
	if (prev == 1)
		signal_free(s);

	return prev - 1;
}

struct signal * signal_copy(struct signal *s)
{
	struct signal *ns;

	ns = alloc(sizeof(struct signal));
	if (!ns)
		return NULL;

	signal_init(ns);

	ns->type = s->type;
	ns->init = s->init;
	ns->enabled = s->enabled;

	if (s->name)
		ns->name = strdup(s->name);

	if (s->unit)
		ns->name = strdup(s->unit);

	return ns;
}

int signal_parse(struct signal *s, json_t *cfg)
{
	int ret;
	json_error_t err;
	json_t *json_init = NULL;
	const char *name = NULL;
	const char *unit = NULL;
	const char *type = NULL;

	ret = json_unpack_ex(cfg, &err, 0, "{ s?: s, s?: s, s?: s, s?: o, s?: b }",
		"name", &name,
		"unit", &unit,
		"type", &type,
		"init", &json_init,
		"enabled", &s->enabled
	);
	if (ret)
		return -1;

	if (name)
		s->name = strdup(name);

	if (unit)
		s->unit = strdup(unit);

	if (type) {
		s->type = signal_type_from_str(type);
		if (s->type == SIGNAL_TYPE_INVALID)
			return -1;
	}

	if (json_init) {
		ret = signal_data_parse_json(&s->init, s, json_init);
		if (ret)
			return ret;
	}
	else
		signal_data_set(&s->init, s, 0);

	return 0;
}

/* Signal list */

int signal_list_parse(struct vlist *list, json_t *cfg)
{
	int ret;
	struct signal *s;

	if (!json_is_array(cfg))
		return -1;

	size_t i;
	json_t *json_signal;
	json_array_foreach(cfg, i, json_signal) {
		s = alloc(sizeof(struct signal));
		if (!s)
			return -1;

		ret = signal_init(s);
		if (ret)
			return ret;

		ret = signal_parse(s, json_signal);
		if (ret)
			return ret;

		vlist_push(list, s);
	}

	return 0;
}

int signal_list_generate(struct vlist *list, unsigned len, enum signal_type fmt)
{
	for (int i = 0; i < len; i++) {
		char name[32];
		snprintf(name, sizeof(name), "signal%d", i);

		struct signal *sig = signal_create(name, NULL, fmt);
		if (!sig)
			return -1;

		vlist_push(list, sig);
	}

	return 0;
}

void signal_list_dump(const struct vlist *list, const union signal_data *data, int len)
{
	debug(5, "  Signals:");

	for (int i = 0; i < vlist_length(list); i++) {
		struct signal *sig = vlist_at(list, i);

		char *buf = strf("    %d:", i);

		if (sig->name)
			strcatf(&buf, " %s", sig->name);

		if (sig->unit)
			strcatf(&buf, " [%s]", sig->unit);

		strcatf(&buf, "(%s)", signal_type_to_str(sig->type));

		if (data && i < len) {
			char val[32];

			signal_data_snprint(&data[i], sig, val, sizeof(val));

			strcatf(&buf, " = %s", val);
		}

		debug(5, "%s", buf);
		free(buf);
	}
}

/* Signal type */

enum signal_type signal_type_from_str(const char *str)
{
	if      (!strcmp(str, "boolean"))
		return SIGNAL_TYPE_BOOLEAN;
	else if (!strcmp(str, "complex"))
		return SIGNAL_TYPE_COMPLEX;
	else if (!strcmp(str, "float"))
		return SIGNAL_TYPE_FLOAT;
	else if (!strcmp(str, "integer"))
		return SIGNAL_TYPE_INTEGER;
	else if (!strcmp(str, "auto"))
		return SIGNAL_TYPE_AUTO;
	else
		return SIGNAL_TYPE_INVALID;
}

const char * signal_type_to_str(enum signal_type fmt)
{
	switch (fmt) {
		case SIGNAL_TYPE_BOOLEAN:
			return "boolean";

		case SIGNAL_TYPE_COMPLEX:
			return "complex";

		case SIGNAL_TYPE_FLOAT:
			return "float";

		case SIGNAL_TYPE_INTEGER:
			return "integer";

		case SIGNAL_TYPE_AUTO:
			return "auto";

		case SIGNAL_TYPE_INVALID:
			return "invalid";
	}

	return NULL;
}

enum signal_type signal_type_detect(const char *val)
{
	char *brk;
	int len;

	debug(LOG_IO | 5, "Attempt to detect type of value: %s", val);

	brk = strchr(val, 'i');
	if (brk)
		return SIGNAL_TYPE_COMPLEX;

	brk = strchr(val, '.');
	if (brk)
		return SIGNAL_TYPE_FLOAT;

	len = strlen(val);
	if (len == 1 && (val[0] == '1' || val[0] == '0'))
		return SIGNAL_TYPE_BOOLEAN;

	return SIGNAL_TYPE_INTEGER;
}

/* Signal data */

void signal_data_set(union signal_data *data, const struct signal *sig, double val)
{
	switch (sig->type) {
		case SIGNAL_TYPE_BOOLEAN:
			data->b = val;
			break;

		case SIGNAL_TYPE_FLOAT:
			data->f = val;
			break;

		case SIGNAL_TYPE_INTEGER:
			data->i = val;
			break;

		case SIGNAL_TYPE_COMPLEX:
			data->z = val;
			break;

		case SIGNAL_TYPE_INVALID:
		case SIGNAL_TYPE_AUTO:
			memset(data, 0, sizeof(union signal_data));
			break;
	}
}

void signal_data_cast(union signal_data *data, const struct signal *from, const struct signal *to)
{
	if (from->type == to->type) /* Nothing to do */
		return;

	switch (to->type) {
		case SIGNAL_TYPE_BOOLEAN:
			switch(from->type) {
				case SIGNAL_TYPE_BOOLEAN:
					data->b = data->b;
					break;

				case SIGNAL_TYPE_INTEGER:
					data->b = data->i;
					break;

				case SIGNAL_TYPE_FLOAT:
					data->b = data->f;
					break;

				case SIGNAL_TYPE_COMPLEX:
					data->b = creal(data->z);
					break;

				default: { }
			}
			break;

		case SIGNAL_TYPE_INTEGER:
			switch(from->type) {
				case SIGNAL_TYPE_BOOLEAN:
					data->i = data->b;
					break;

				case SIGNAL_TYPE_INTEGER:
					data->i = data->i;
					break;

				case SIGNAL_TYPE_FLOAT:
					data->i = data->f;
					break;

				case SIGNAL_TYPE_COMPLEX:
					data->i = creal(data->z);
					break;

				default: { }
			}
			break;

		case SIGNAL_TYPE_FLOAT:
			switch(from->type) {
				case SIGNAL_TYPE_BOOLEAN:
					data->f = data->b;
					break;

				case SIGNAL_TYPE_INTEGER:
					data->f = data->i;
					break;

				case SIGNAL_TYPE_FLOAT:
					data->f = data->f;
					break;

				case SIGNAL_TYPE_COMPLEX:
					data->f = creal(data->z);
					break;

				default: { }
			}
			break;

		case SIGNAL_TYPE_COMPLEX:
			switch(from->type) {
				case SIGNAL_TYPE_BOOLEAN:
					data->z = CMPLXF(data->b, 0);
					break;

				case SIGNAL_TYPE_INTEGER:
					data->z = CMPLXF(data->i, 0);
					break;

				case SIGNAL_TYPE_FLOAT:
					data->z = CMPLXF(data->f, 0);
					break;

				case SIGNAL_TYPE_COMPLEX:
					data->z = data->z;
					break;

				default: { }
			}
			break;

		default: { }
	}
}

int signal_data_parse_str(union signal_data *data, const struct signal *sig, const char *ptr, char **end)
{
	switch (sig->type) {
		case SIGNAL_TYPE_FLOAT:
			data->f = strtod(ptr, end);
			break;

		case SIGNAL_TYPE_INTEGER:
			data->i = strtol(ptr, end, 10);
			break;

		case SIGNAL_TYPE_BOOLEAN:
			data->b = strtol(ptr, end, 10);
			break;

		case SIGNAL_TYPE_COMPLEX: {
			float real, imag;

			real = strtod(ptr, end);
			if (*end == ptr)
				return -1;

			ptr = *end;

			imag = strtod(ptr, end);
			if (*end == ptr)
				return -1;

			if (**end != 'i')
				return -1;

			(*end)++;

			data->z = CMPLXF(real, imag);
			break;
		}

		case SIGNAL_TYPE_AUTO:
		case SIGNAL_TYPE_INVALID:
			return -1;
	}

	return 0;
}

int signal_data_parse_json(union signal_data *data, const struct signal *sig, json_t *cfg)
{
	int ret;

	switch (sig->type) {
		case SIGNAL_TYPE_FLOAT:
			data->f = json_real_value(cfg);
			break;

		case SIGNAL_TYPE_INTEGER:
			data->i = json_integer_value(cfg);
			break;

		case SIGNAL_TYPE_BOOLEAN:
			data->b = json_boolean_value(cfg);
			break;

		case SIGNAL_TYPE_COMPLEX: {
			double real, imag;

			ret = json_unpack(cfg, "{ s: F, s: F }",
				"real", &real,
				"imag", &imag
			);
			if (ret)
				return -2;

			data->z = CMPLXF(real, imag);
			break;
		}

		case SIGNAL_TYPE_INVALID:
		case SIGNAL_TYPE_AUTO:
			return -1;
	}

	return 0;
}

int signal_data_snprint(const union signal_data *data, const struct signal *sig, char *buf, size_t len)
{
	switch (sig->type) {
		case SIGNAL_TYPE_FLOAT:
			return snprintf(buf, len, "%.6f", data->f);

		case SIGNAL_TYPE_INTEGER:
			return snprintf(buf, len, "%" PRIi64, data->i);

		case SIGNAL_TYPE_BOOLEAN:
			return snprintf(buf, len, "%u", data->b);

		case SIGNAL_TYPE_COMPLEX:
			return snprintf(buf, len, "%.6f%+.6fi", creal(data->z), cimag(data->z));

		default:
			return 0;
	}
}
