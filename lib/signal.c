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

#include "villas/signal.h"
#include "villas/list.h"
#include "villas/utils.h"
#include "villas/node.h"

int signal_destroy(struct signal *s)
{
	if (s->name)
		free(s->name);

	return 0;
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
