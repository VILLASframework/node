/** Signal meta data.
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

#include <string.h>

#include <villas/signal.h>
#include <villas/list.h>
#include <villas/utils.h>
#include <villas/node.h>
#include <villas/mapping.h>

int signal_init(struct signal *s)
{
	s->name = NULL;
	s->unit = NULL;
	s->format = SIGNAL_FORMAT_UNKNOWN;

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
				case MAPPING_STATS_TYPE_TOTAL:
					s->format = SIGNAL_FORMAT_INT;
					break;

				case MAPPING_STATS_TYPE_LAST:
				case MAPPING_STATS_TYPE_LOWEST:
				case MAPPING_STATS_TYPE_HIGHEST:
				case MAPPING_STATS_TYPE_MEAN:
				case MAPPING_STATS_TYPE_VAR:
				case MAPPING_STATS_TYPE_STDDEV:
					s->format = SIGNAL_FORMAT_FLOAT;
					break;
			}
			break;

		case MAPPING_TYPE_HEADER:
			switch (me->hdr.type) {
				case MAPPING_HEADER_TYPE_LENGTH:
				case MAPPING_HEADER_TYPE_SEQUENCE:
					s->format = SIGNAL_FORMAT_INT;
					break;
			}
			break;

		case MAPPING_TYPE_TIMESTAMP:
			s->format = SIGNAL_FORMAT_INT;
			break;

		case MAPPING_TYPE_DATA:
			s->format = me->signal->format;
			s->unit = me->signal->unit;
			break;
	}

	return 0;
}

int signal_destroy(struct signal *s)
{
	if (s->name)
		free(s->name);

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
		signal_destroy(s);

	return prev - 1;
}

int signal_parse(struct signal *s, json_t *cfg)
{
	int ret;
	json_error_t err;
	const char *name;
	const char *unit = NULL;

	/* Default values */
	s->enabled = true;

	ret = json_unpack_ex(cfg, &err, 0, "{ s: s, s?: s, s?: b }",
		"name", &name,
		"unit", &unit,
		"enabled", &s->enabled
	);
	if (ret)
		return -1;

	s->name = strdup(name);
	s->unit = unit
		? strdup(unit)
		: NULL;

	return 0;
}

int signal_parse_list(struct list *list, json_t *cfg)
{
	int ret;
	struct signal *s;

	if (!json_is_array(cfg))
		return -1;

	size_t index;
	json_t *json_signal;
	json_array_foreach(cfg, index, json_signal) {
		s = alloc(sizeof(struct signal));
		if (!s)
			return -1;

		ret = signal_parse(s, json_signal);
		if (ret)
			return -1;

		list_push(list, s);
	}

	return 0;
}

int signal_get_offsets(const char *str, struct list *sigs)
{
	int idx;
	char *endptr;
	struct signal *s;

	/* Lets try to find a signal with a matching name */
	if (1) {
		s = list_lookup(sigs, str);
		if (s)
			return list_index(sigs, s);
	}

	/* Lets try to interpret the signal name as an index */
	idx = strtoul(str, &endptr, 10);
	if (endptr == str + strlen(str))
		return idx;

	return -1;
}
