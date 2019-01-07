/** Sample value remapping for mux.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/mapping.h>
#include <villas/stats.h>
#include <villas/sample.h>
#include <villas/list.h>
#include <villas/utils.h>
#include <villas/node.h>
#include <villas/signal.h>

int mapping_parse_str(struct mapping_entry *me, const char *str, struct vlist *nodes)
{
	int id;
	char *cpy, *node, *type, *field, *subfield, *end;

	cpy = strdup(str);
	if (!cpy)
		return -1;

	if (nodes) {
		node = strtok(cpy, ".");
		if (!node) {
			warning("Missing node name");
			goto invalid_format;
		}

		me->node = vlist_lookup(nodes, node);
		if (!me->node) {
			warning("Unknown node %s", node);
			goto invalid_format;
		}

		type = strtok(NULL, ".[");
		if (!type)
			type = "data";
	}
	else {
		me->node = NULL;

		type = strtok(cpy, ".[");
		if (!type)
			goto invalid_format;
	}

	if      (!strcmp(type, "stats")) {
		me->type = MAPPING_TYPE_STATS;
		me->length = 1;

		field = strtok(NULL, ".");
		if (!field) {
			warning("Missing stats type");
			goto invalid_format;
		}

		subfield = strtok(NULL, ".");
		if (!subfield) {
			warning("Missing stats sub-type");
			goto invalid_format;
		}

		id = stats_lookup_id(field);
		if (id < 0) {
			warning("Invalid stats type");
			goto invalid_format;
		}

		me->stats.id = id;

		if      (!strcmp(subfield, "total"))
			me->stats.type = MAPPING_STATS_TYPE_TOTAL;
		else if (!strcmp(subfield, "last"))
			me->stats.type = MAPPING_STATS_TYPE_LAST;
		else if (!strcmp(subfield, "lowest"))
			me->stats.type = MAPPING_STATS_TYPE_LOWEST;
		else if (!strcmp(subfield, "highest"))
			me->stats.type = MAPPING_STATS_TYPE_HIGHEST;
		else if (!strcmp(subfield, "mean"))
			me->stats.type = MAPPING_STATS_TYPE_MEAN;
		else if (!strcmp(subfield, "var"))
			me->stats.type = MAPPING_STATS_TYPE_VAR;
		else if (!strcmp(subfield, "stddev"))
			me->stats.type = MAPPING_STATS_TYPE_STDDEV;
		else {
			warning("Invalid stats sub-type");
			goto invalid_format;
		}
	}
	else if (!strcmp(type, "hdr")) {
		me->type = MAPPING_TYPE_HEADER;
		me->length = 1;

		field = strtok(NULL, ".");
		if (!field) {
			warning("Missing header type");
			goto invalid_format;
		}

		if      (!strcmp(field, "sequence"))
			me->header.type = MAPPING_HEADER_TYPE_SEQUENCE;
		else if (!strcmp(field, "length"))
			me->header.type = MAPPING_HEADER_TYPE_LENGTH;
		else {
			warning("Invalid header type");
			goto invalid_format;
		}
	}
	else if (!strcmp(type, "ts")) {
		me->type = MAPPING_TYPE_TIMESTAMP;
		me->length = 2;

		field = strtok(NULL, ".");
		if (!field) {
			warning("Missing timestamp type");
			goto invalid_format;
		}

		if      (!strcmp(field, "origin"))
			me->timestamp.type = MAPPING_TIMESTAMP_TYPE_ORIGIN;
		else if (!strcmp(field, "received"))
			me->timestamp.type = MAPPING_TIMESTAMP_TYPE_RECEIVED;
		else {
			warning("Invalid timestamp type");
			goto invalid_format;
		}
	}
	else if (!strcmp(type, "data")) {
		char *first_str, *last_str;
		int first = -1, last = -1;

		me->type = MAPPING_TYPE_DATA;

		first_str = strtok(NULL, "-]");
		if (first_str) {
			if (me->node)
				first = vlist_lookup_index(&me->node->signals, first_str);

			if (first < 0) {
				char *endptr;
				first = strtoul(first_str, &endptr, 10);
				if (endptr != first_str + strlen(first_str)) {
					warning("Failed to parse data range");
					goto invalid_format;
				}
			}
		}
		else {
			/* Map all signals */
			me->data.offset = 0;
			me->length = me->node ? vlist_length(&me->node->signals) : 0;
			goto end;
		}

		last_str = strtok(NULL, "]");
		if (last_str) {
			if (me->node)
				last = vlist_lookup_index(&me->node->signals, last_str);

			if (last < 0) {
				char *endptr;
				last = strtoul(last_str, &endptr, 10);
				if (endptr != last_str + strlen(last_str)) {
					warning("Failed to parse data range");
					goto invalid_format;
				}
			}
		}
		else
			last = first; /* single element: data[5] => data[5-5] */

		if (last < first)
			goto invalid_format;

		me->data.offset = first;
		me->length = last - first + 1;
	}
	else
		goto invalid_format;

end:	/* Check that there is no garbage at the end */
	end = strtok(NULL, "");
	if (end)
		goto invalid_format;

	free(cpy);

	return 0;

invalid_format:

	free(cpy);

	return -1;
}

int mapping_parse(struct mapping_entry *me, json_t *cfg, struct vlist *nodes)
{
	const char *str;

	str = json_string_value(cfg);
	if (!str)
		return -1;

	return mapping_parse_str(me, str, nodes);
}

int mapping_parse_list(struct vlist *l, json_t *cfg, struct vlist *nodes)
{
	int ret, off;

	size_t i;
	json_t *json_entry;
	json_t *json_mapping;

	if (json_is_string(cfg)) {
		json_mapping = json_array();
		json_array_append(json_mapping, cfg);
	}
	else if (json_is_array(cfg))
		json_mapping = json_incref(cfg);
	else
		return -1;

	off = 0;
	json_array_foreach(json_mapping, i, json_entry) {
		struct mapping_entry *me = (struct mapping_entry *) alloc(sizeof(struct mapping_entry));

		ret = mapping_parse(me, json_entry, nodes);
		if (ret)
			goto out;

		me->offset = off;
		off += me->length;

		vlist_push(l, me);
	}

	ret = 0;

out:	json_decref(json_mapping);

	return ret;
}

int mapping_update(const struct mapping_entry *me, struct sample *remapped, const struct sample *original, const struct stats *s)
{
	int len = me->length;
	int off = me->offset;

	/* me->length == 0 means that we want to take all values */
	if (!len)
		len = original->length;

	if (len + off > remapped->capacity)
		return -1;

	switch (me->type) {
		case MAPPING_TYPE_STATS: {
			const struct hist *h = &s->histograms[me->stats.id];

			switch (me->stats.type) {
				case MAPPING_STATS_TYPE_TOTAL:
					remapped->data[off++].i = h->total;
					break;
				case MAPPING_STATS_TYPE_LAST:
					remapped->data[off++].f = h->last;
					break;
				case MAPPING_STATS_TYPE_HIGHEST:
					remapped->data[off++].f = h->highest;
					break;
				case MAPPING_STATS_TYPE_LOWEST:
					remapped->data[off++].f = h->lowest;
					break;
				case MAPPING_STATS_TYPE_MEAN:
					remapped->data[off++].f = hist_mean(h);
					break;
				case MAPPING_STATS_TYPE_STDDEV:
					remapped->data[off++].f = hist_stddev(h);
					break;
				case MAPPING_STATS_TYPE_VAR:
					remapped->data[off++].f = hist_var(h);
					break;
				default:
					return -1;
			}
		}

		case MAPPING_TYPE_TIMESTAMP: {
			const struct timespec *ts;

			switch (me->timestamp.type) {
				case MAPPING_TIMESTAMP_TYPE_RECEIVED:
					ts = &original->ts.received;
					break;
				case MAPPING_TIMESTAMP_TYPE_ORIGIN:
					ts = &original->ts.origin;
					break;
				default:
					return -1;
			}

			remapped->data[off++].i = ts->tv_sec;
			remapped->data[off++].i = ts->tv_nsec;

			break;
		}

		case MAPPING_TYPE_HEADER:
			switch (me->header.type) {
				case MAPPING_HEADER_TYPE_LENGTH:
					remapped->data[off++].i = original->length;
					break;
				case MAPPING_HEADER_TYPE_SEQUENCE:
					remapped->data[off++].i = original->sequence;
					break;
				default:
					return -1;
			}

			break;

		case MAPPING_TYPE_DATA:
			for (int j = me->data.offset; j < len + me->data.offset; j++) {
				if (j >= original->length)
					remapped->data[off++].f = 0;
				else
					remapped->data[off++] = original->data[j];
			}

			break;
	}

	return 0;
}

int mapping_remap(const struct vlist *m, struct sample *remapped, const struct sample *original, const struct stats *s)
{
	int ret;

	for (size_t i = 0; i < vlist_length(m); i++) {
		struct mapping_entry *me = (struct mapping_entry *) vlist_at(m, i);

		ret = mapping_update(me, remapped, original, s);
		if (ret)
			return ret;
	}

	return 0;
}

int mapping_to_str(const struct mapping_entry *me, unsigned index, char **str)
{
	const char *type;

	assert(me->length == 0 || index < me->length);

	if (me->node)
		strcatf(str, "%s.", node_name_short(me->node));

	switch (me->type) {
		case MAPPING_TYPE_STATS:
			switch (me->stats.type) {
				case MAPPING_STATS_TYPE_TOTAL:
					type = "total";
					break;

				case MAPPING_STATS_TYPE_LAST:
					type = "last";
					break;

				case MAPPING_STATS_TYPE_LOWEST:
					type = "lowest";
					break;

				case MAPPING_STATS_TYPE_HIGHEST:
					type = "highest";
					break;

				case MAPPING_STATS_TYPE_MEAN:
					type = "mean";
					break;

				case MAPPING_STATS_TYPE_VAR:
					type = "var";
					break;

				case MAPPING_STATS_TYPE_STDDEV:
					type = "stddev";
					break;

				default:
					type = NULL;
			}

			strcatf(str, "stats.%s", type);
			break;

		case MAPPING_TYPE_HEADER:
			switch (me->header.type) {
				case MAPPING_HEADER_TYPE_LENGTH:
					type = "length";
					break;

				case MAPPING_HEADER_TYPE_SEQUENCE:
					type = "sequence";
					break;

				default:
					type = NULL;
			}

			strcatf(str, "hdr.%s", type);
			break;

		case MAPPING_TYPE_TIMESTAMP:
			switch (me->timestamp.type) {
				case MAPPING_TIMESTAMP_TYPE_ORIGIN:
					type = "origin";
					break;

				case MAPPING_TIMESTAMP_TYPE_RECEIVED:
					type = "received";
					break;

				default:
					type = NULL;
			}

			strcatf(str, "ts.%s.%s", type, index == 0 ? "sec" : "nsec");
			break;

		case MAPPING_TYPE_DATA:
			if (me->node && index < vlist_length(&me->node->signals)) {
				struct signal *s = vlist_at(&me->node->signals, index);

				strcatf(str, "data[%s]", s->name);
			}
			else
				strcatf(str, "data[%u]", index);
			break;
	}

	return 0;
}
