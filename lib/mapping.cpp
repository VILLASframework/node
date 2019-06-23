/** Sample value remapping for mux.
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

#include <villas/mapping.h>
#include <villas/sample.h>
#include <villas/list.h>
#include <villas/utils.hpp>
#include <villas/node.h>
#include <villas/signal.h>

using namespace villas;
using namespace villas::utils;

int mapping_parse_str(struct mapping_entry *me, const char *str, struct vlist *nodes)
{
	char *cpy, *node, *type, *field, *end, *lasts;

	cpy = strdup(str);
	if (!cpy)
		return -1;

	if (nodes) {
		node = strtok_r(cpy, ".", &lasts);
		if (!node) {
			warning("Missing node name");
			goto invalid_format;
		}

		me->node = (struct node *) vlist_lookup(nodes, node);
		if (!me->node) {
			warning("Unknown node %s", node);
			goto invalid_format;
		}

		type = strtok_r(nullptr, ".[", &lasts);
		if (!type)
			type = strf("data");
	}
	else {
		me->node = nullptr;

		type = strtok_r(cpy, ".[", &lasts);
		if (!type)
			goto invalid_format;
	}

	if      (!strcmp(type, "stats")) {
		me->type = MappingType::STATS;
		me->length = 1;

		char *metric = strtok_r(nullptr, ".", &lasts);
		if (!metric)
			goto invalid_format;

		type = strtok_r(nullptr, ".", &lasts);
		if (!type)
			goto invalid_format;

		me->stats.metric = Stats::lookupMetric(metric);
		me->stats.type = Stats::lookupType(type);
	}
	else if (!strcmp(type, "hdr")) {
		me->type = MappingType::HEADER;
		me->length = 1;

		field = strtok_r(nullptr, ".", &lasts);
		if (!field) {
			warning("Missing header type");
			goto invalid_format;
		}

		if      (!strcmp(field, "sequence"))
			me->header.type = MappingHeaderType::SEQUENCE;
		else if (!strcmp(field, "length"))
			me->header.type = MappingHeaderType::LENGTH;
		else {
			warning("Invalid header type");
			goto invalid_format;
		}
	}
	else if (!strcmp(type, "ts")) {
		me->type = MappingType::TIMESTAMP;
		me->length = 2;

		field = strtok_r(nullptr, ".", &lasts);
		if (!field) {
			warning("Missing timestamp type");
			goto invalid_format;
		}

		if      (!strcmp(field, "origin"))
			me->timestamp.type = MappingTimestampType::ORIGIN;
		else if (!strcmp(field, "received"))
			me->timestamp.type = MappingTimestampType::RECEIVED;
		else {
			warning("Invalid timestamp type");
			goto invalid_format;
		}
	}
	else if (!strcmp(type, "data")) {
		char *first_str, *last_str;
		int first = -1, last = -1;

		me->type = MappingType::DATA;

		first_str = strtok_r(nullptr, "-]", &lasts);
		if (first_str) {
			if (me->node)
				first = vlist_lookup_index(&me->node->in.signals, first_str);

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
			me->length = -1;
			goto end;
		}

		last_str = strtok_r(nullptr, "]", &lasts);
		if (last_str) {
			if (me->node)
				last = vlist_lookup_index(&me->node->in.signals, last_str);

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
	end = strtok_r(nullptr, "", &lasts);
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

int mapping_list_parse(struct vlist *ml, json_t *cfg, struct vlist *nodes)
{
	int ret;

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

	json_array_foreach(json_mapping, i, json_entry) {
		struct mapping_entry *me = (struct mapping_entry *) alloc(sizeof(struct mapping_entry));

		ret = mapping_parse(me, json_entry, nodes);
		if (ret)
			goto out;

		vlist_push(ml, me);
	}

	ret = 0;

out:	json_decref(json_mapping);

	return ret;
}

int mapping_update(const struct mapping_entry *me, struct sample *remapped, const struct sample *original)
{
	unsigned len = me->length;

	if (me->offset + len > remapped->capacity)
		return -1;

	switch (me->type) {
		case MappingType::STATS:
			remapped->data[me->offset] = me->node->stats->getValue(me->stats.metric, me->stats.type);
			break;

		case MappingType::TIMESTAMP: {
			const struct timespec *ts;

			switch (me->timestamp.type) {
				case MappingTimestampType::RECEIVED:
					ts = &original->ts.received;
					break;
				case MappingTimestampType::ORIGIN:
					ts = &original->ts.origin;
					break;
				default:
					return -1;
			}

			remapped->data[me->offset + 0].i = ts->tv_sec;
			remapped->data[me->offset + 1].i = ts->tv_nsec;
			break;
		}

		case MappingType::HEADER:
			switch (me->header.type) {
				case MappingHeaderType::LENGTH:
					remapped->data[me->offset].i = original->length;
					break;

				case MappingHeaderType::SEQUENCE:
					remapped->data[me->offset].i = original->sequence;
					break;

				default:
					return -1;
			}
			break;

		case MappingType::DATA:
			for (unsigned j = me->data.offset,
				 i = me->offset;
			         j < MIN(original->length, (unsigned) (me->data.offset + me->length));
				 j++,
				 i++)
			{
				if (j >= original->length)
					remapped->data[i].f = -1;
				else
					remapped->data[i] = original->data[j];
			}

			len = MIN((unsigned) me->length, original->length - me->data.offset);
			break;
	}

	if (me->offset + len > remapped->length)
		remapped->length = me->offset + len;

	return 0;
}

int mapping_list_remap(const struct vlist *ml, struct sample *remapped, const struct sample *original)
{
	int ret;

	for (size_t i = 0; i < vlist_length(ml); i++) {
		struct mapping_entry *me = (struct mapping_entry *) vlist_at(ml, i);

		ret = mapping_update(me, remapped, original);
		if (ret)
			return ret;
	}

	return 0;
}

int mapping_list_prepare(struct vlist *ml)
{
	for (size_t i = 0, off = 0; i < vlist_length(ml); i++) {
		struct mapping_entry *me = (struct mapping_entry *) vlist_at(ml, i);

		if (me->length < 0) {
			struct vlist *sigs = node_get_signals(me->node, NodeDir::IN);

			me->length = vlist_length(sigs);
		}

		me->offset = off;
		off += me->length;
	}

	return 0;
}

int mapping_to_str(const struct mapping_entry *me, unsigned index, char **str)
{
	const char *type;

	assert(me->length == 0 || (int) index < me->length);

	if (me->node)
		strcatf(str, "%s.", node_name_short(me->node));

	switch (me->type) {
		case MappingType::STATS:
			strcatf(str, "stats.%s.%s",
				Stats::metrics[me->stats.metric].name,
				Stats::types[me->stats.type].name
			);
			break;

		case MappingType::HEADER:
			switch (me->header.type) {
				case MappingHeaderType::LENGTH:
					type = "length";
					break;

				case MappingHeaderType::SEQUENCE:
					type = "sequence";
					break;

				default:
					type = nullptr;
			}

			strcatf(str, "hdr.%s", type);
			break;

		case MappingType::TIMESTAMP:
			switch (me->timestamp.type) {
				case MappingTimestampType::ORIGIN:
					type = "origin";
					break;

				case MappingTimestampType::RECEIVED:
					type = "received";
					break;

				default:
					type = nullptr;
			}

			strcatf(str, "ts.%s.%s", type, index == 0 ? "sec" : "nsec");
			break;

		case MappingType::DATA:
			if (me->node && index < vlist_length(&me->node->in.signals)) {
				struct signal *s = (struct signal *) vlist_at(&me->node->in.signals, index);

				strcatf(str, "data[%s]", s->name);
			}
			else
				strcatf(str, "data[%u]", index);
			break;
	}

	return 0;
}
