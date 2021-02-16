/** Sample value remapping for mux.
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

#include <regex>
#include <iostream>

#include <villas/mapping.h>
#include <villas/sample.h>
#include <villas/list.h>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/node.h>
#include <villas/signal.h>

using namespace villas;
using namespace villas::utils;

int mapping_entry_parse_str(struct mapping_entry *me, const std::string &str)
{
	std::smatch mr;
	std::regex re(RE_MAPPING);

	if (!std::regex_match(str, mr, re))
		goto invalid_format;

	if (mr[1].matched)
		me->node_name = strdup(mr.str(1).c_str());

	if (mr[9].matched)
		me->node_name = strdup(mr.str(9).c_str());

	if (mr[6].matched) {
		me->data.first = strdup(mr.str(6).c_str());
		me->data.last = mr[7].matched
			? strdup(mr.str(7).c_str())
			: nullptr;

		me->type = MappingType::DATA;
	}
	else if (mr[10].matched) {
		me->data.first = strdup(mr.str(10).c_str());
		me->data.last = mr[11].matched
			? strdup(mr.str(11).c_str())
			: nullptr;

		me->type = MappingType::DATA;
	}
	else if (mr[8].matched) {
		me->data.first = strdup(mr.str(8).c_str());
		me->data.last = nullptr;

		me->type = MappingType::DATA;
	}
	else if (mr[2].matched) {
		me->stats.type   = Stats::lookupType(mr.str(3));
		me->stats.metric = Stats::lookupMetric(mr.str(2));

		me->type = MappingType::STATS;
	}
	else if (mr[5].matched) {
		if      (mr.str(5) == "origin")
			me->timestamp.type = MappingTimestampType::ORIGIN;
		else if (mr.str(5) == "received")
			me->timestamp.type = MappingTimestampType::RECEIVED;
		else
			goto invalid_format;

		me->type = MappingType::TIMESTAMP;
	}
	else if (mr[4].matched) {
		if      (mr.str(4) == "sequence")
			me->header.type = MappingHeaderType::SEQUENCE;
		else if (mr.str(4) == "length")
			me->header.type = MappingHeaderType::LENGTH;
		else
			goto invalid_format;

		me->type = MappingType::HEADER;
	}
	/* Only node name given.. We map all data */
	else if (me->node_name) {
		me->data.first = nullptr;
		me->data.last = nullptr;

		me->type = MappingType::DATA;
	}

	return 0;

invalid_format:

	throw RuntimeError("Failed to parse mapping expression: {}", str);
}

int mapping_entry_init(struct mapping_entry *me)
{
	me->type = MappingType::UNKNOWN;

	me->node = nullptr;
	me->node_name = nullptr;

	return 0;
}

int mapping_entry_destroy(struct mapping_entry *me)
{
	if (me->node_name)
		free(me->node_name);

	return 0;
}

int mapping_entry_parse(struct mapping_entry *me, json_t *json)
{
	const char *str;

	str = json_string_value(json);
	if (!str)
		return -1;

	return mapping_entry_parse_str(me, str);
}

int mapping_list_parse(struct vlist *ml, json_t *json)
{
	int ret;

	size_t i;
	json_t *json_entry;
	json_t *json_mapping;

	if (json_is_string(json)) {
		json_mapping = json_array();
		json_array_append(json_mapping, json);
	}
	else if (json_is_array(json))
		json_mapping = json_incref(json);
	else
		return -1;

	json_array_foreach(json_mapping, i, json_entry) {
		auto *me = new struct mapping_entry;
		if (!me)
			throw MemoryAllocationError();

		ret = mapping_entry_init(me);
		if (ret)
			goto out;

		ret = mapping_entry_parse(me, json_entry);
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

		case MappingType::UNKNOWN:
			return -1;
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

int mapping_entry_prepare(struct mapping_entry *me, struct vlist *nodes)
{
	if (me->node_name && me->node == nullptr) {
		me->node = vlist_lookup_name<struct vnode>(nodes, me->node_name);
		if (!me->node)
			throw RuntimeError("Invalid node name in mapping: {}", me->node_name);
	}

	if (me->type == MappingType::DATA) {
		int first = -1, last = -1;

		if (me->data.first) {
			if (me->node)
				first = vlist_lookup_index<struct signal>(&me->node->in.signals, me->data.first);

			if (first < 0) {
				char *endptr;
				first = strtoul(me->data.first, &endptr, 10);
				if (endptr != me->data.first + strlen(me->data.first))
					throw RuntimeError("Failed to parse data index in mapping: {}", me->data.first);
			}
		}
		else {
			/* Map all signals */
			me->data.offset = 0;
			me->length = -1;
			goto end;
		}

		if (me->data.last) {
			if (me->node)
				last = vlist_lookup_index<struct signal>(&me->node->in.signals, me->data.last);

			if (last < 0) {
				char *endptr;
				last = strtoul(me->data.last, &endptr, 10);
				if (endptr != me->data.last + strlen(me->data.last))
					throw RuntimeError("Failed to parse data index in mapping: {}", me->data.last);
			}
		}
		else
			last = first; /* single element: data[5] => data[5-5] */

		if (last < first)
			throw RuntimeError("Invalid data range indices for mapping: {} < {}", last, first);

		me->data.offset = first;
		me->length = last - first + 1;
	}

end:
	if (me->length < 0) {
		struct vlist *sigs = node_input_signals(me->node);

		me->length = vlist_length(sigs);
	}

	return 0;
}

int mapping_list_prepare(struct vlist *ml, struct vlist *nodes)
{
	int ret;

	for (size_t i = 0, off = 0; i < vlist_length(ml); i++) {
		struct mapping_entry *me = (struct mapping_entry *) vlist_at(ml, i);

		ret = mapping_entry_prepare(me, nodes);
		if (ret)
			return ret;

		me->offset = off;
		off += me->length;
	}

	return 0;
}

int mapping_entry_to_str(const struct mapping_entry *me, unsigned index, char **str)
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

		case MappingType::UNKNOWN:
			return -1;
	}

	return 0;
}
