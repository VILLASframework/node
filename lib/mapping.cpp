/** Sample value remapping for mux.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/mapping.hpp>
#include <villas/sample.hpp>
#include <villas/list.hpp>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/node.hpp>
#include <villas/signal.hpp>

using namespace villas;
using namespace villas::node;
using namespace villas::utils;

int MappingEntry::parseString(const std::string &str)
{
	std::smatch mr;
	std::regex re(RE_MAPPING);

	if (!std::regex_match(str, mr, re))
		goto invalid_format;

	if (mr[1].matched)
		nodeName = mr.str(1);

	if (mr[9].matched)
		nodeName = mr.str(9);

	if (mr[6].matched) {
		data.first = strdup(mr.str(6).c_str());
		data.last = mr[7].matched
			? strdup(mr.str(7).c_str())
			: nullptr;

		type = Type::DATA;
	}
	else if (mr[10].matched) {
		data.first = strdup(mr.str(10).c_str());
		data.last = mr[11].matched
			? strdup(mr.str(11).c_str())
			: nullptr;

		type = Type::DATA;
	}
	else if (mr[8].matched) {
		data.first = strdup(mr.str(8).c_str());
		data.last = nullptr;

		type = Type::DATA;
	}
	else if (mr[2].matched) {
		stats.type   = Stats::lookupType(mr.str(3));
		stats.metric = Stats::lookupMetric(mr.str(2));

		type = Type::STATS;
	}
	else if (mr[5].matched) {
		if      (mr.str(5) == "origin")
			timestamp.type = TimestampType::ORIGIN;
		else if (mr.str(5) == "received")
			timestamp.type = TimestampType::RECEIVED;
		else
			goto invalid_format;

		type = Type::TIMESTAMP;
	}
	else if (mr[4].matched) {
		if      (mr.str(4) == "sequence")
			header.type = HeaderType::SEQUENCE;
		else if (mr.str(4) == "length")
			header.type = HeaderType::LENGTH;
		else
			goto invalid_format;

		type = Type::HEADER;
	}
	/* Only node name given.. We map all data */
	else if (!nodeName.empty()) {
		data.first = nullptr;
		data.last = nullptr;

		type = Type::DATA;
	}

	return 0;

invalid_format:

	throw RuntimeError("Failed to parse mapping expression: {}", str);
}

MappingEntry::MappingEntry() :
	node(nullptr),
	type(Type::UNKNOWN),
	length(0),
	offset(0),
	nodeName()
{ }

int MappingEntry::parse(json_t *json)
{
	const char *str;

	str = json_string_value(json);
	if (!str)
		return -1;

	return parseString(str);
}

int MappingEntry::update(struct Sample *remapped, const struct Sample *original) const
{
	unsigned len = length;

	if (offset + len > remapped->capacity)
		return -1;

	switch (type) {
		case Type::STATS:
			remapped->data[offset] = node->getStats()->getValue(stats.metric, stats.type);
			break;

		case Type::TIMESTAMP: {
			const struct timespec *ts;

			switch (timestamp.type) {
				case TimestampType::RECEIVED:
					ts = &original->ts.received;
					break;
				case TimestampType::ORIGIN:
					ts = &original->ts.origin;
					break;
				default:
					return -1;
			}

			remapped->data[offset + 0].i = ts->tv_sec;
			remapped->data[offset + 1].i = ts->tv_nsec;
			break;
		}

		case Type::HEADER:
			switch (header.type) {
				case HeaderType::LENGTH:
					remapped->data[offset].i = original->length;
					break;

				case HeaderType::SEQUENCE:
					remapped->data[offset].i = original->sequence;
					break;

				default:
					return -1;
			}
			break;

		case Type::DATA:
			for (unsigned j = data.offset,
				 i = offset;
			         j < MIN(original->length, (unsigned) (data.offset + length));
				 j++,
				 i++)
			{
				if (j >= original->length)
					remapped->data[i].f = -1;
				else
					remapped->data[i] = original->data[j];
			}

			len = MIN((unsigned) length, original->length - data.offset);
			break;

		case Type::UNKNOWN:
			return -1;
	}

	if (offset + len > remapped->length)
		remapped->length = offset + len;

	return 0;
}

int MappingEntry::prepare(NodeList &nodes)
{
	if (!nodeName.empty() && node == nullptr) {
		node = nodes.lookup(nodeName);
		if (!node)
			throw RuntimeError("Invalid node name in mapping: {}", nodeName);
	}

	if (type == Type::DATA) {
		int first = -1, last = -1;

		if (data.first) {
			if (node)
				first = node->getInputSignals()->getIndexByName(data.first);

			if (first < 0) {
				char *endptr;
				first = strtoul(data.first, &endptr, 10);
				if (endptr != data.first + strlen(data.first))
					throw RuntimeError("Failed to parse data index in mapping: {}", data.first);
			}
		}
		else {
			/* Map all signals */
			data.offset = 0;
			length = -1;
			goto end;
		}

		if (data.last) {
			if (node)
				last = node->getInputSignals()->getIndexByName(data.last);

			if (last < 0) {
				char *endptr;
				last = strtoul(data.last, &endptr, 10);
				if (endptr != data.last + strlen(data.last))
					throw RuntimeError("Failed to parse data index in mapping: {}", data.last);
			}
		}
		else
			last = first; /* single element: data[5] => data[5-5] */

		if (last < first)
			throw RuntimeError("Invalid data range indices for mapping: {} < {}", last, first);

		data.offset = first;
		length = last - first + 1;
	}
	else {
		length = 1;
	}

end:
	if (length < 0)
		length = node->getInputSignals()->size();

	return 0;
}

std::string MappingEntry::toString(unsigned index) const
{
	assert(length == 0 || (int) index < length);

	std::stringstream ss;

	if (node)
		ss << node->getNameShort() << ".";

	switch (type) {
		case Type::STATS:
			ss << "stats.";
			ss << Stats::metrics[stats.metric].name << ".";
			ss << Stats::types[stats.type].name;
			break;

		case Type::HEADER:
			ss << "hdr.";
			switch (header.type) {
				case HeaderType::LENGTH:
					ss << "length";
					break;

				case HeaderType::SEQUENCE:
					ss << "sequence";
					break;

				default: {}
			}
			break;

		case Type::TIMESTAMP:
			ss << "ts.";
			switch (timestamp.type) {
				case TimestampType::ORIGIN:
					ss << "origin.";
					break;

				case TimestampType::RECEIVED:
					ss << "received.";
					break;

				default: {}
			}

			ss << (index == 0 ? "sec" : "nsec");
			break;

		case Type::DATA:
			if (node && index < node->getInputSignals()->size()) {
				auto s = node->getInputSignals()->getByIndex(index);

				ss << "data[" << s->name << "]";
			}
			else
				ss << "data[" << index << "]";
			break;

		case Type::UNKNOWN:
			return "<unknown>";
	}

	return ss.str();
}


Signal::Ptr MappingEntry::toSignal(unsigned index) const
{
	auto name = toString(index);

	switch (type) {
		case MappingEntry::Type::STATS:
			return std::make_shared<Signal>(name, Stats::metrics[stats.metric].unit,
			                                      Stats::types[stats.type].signal_type);

		case MappingEntry::Type::HEADER:
			switch (header.type) {
				case MappingEntry::HeaderType::LENGTH:
				case MappingEntry::HeaderType::SEQUENCE:
					return std::make_shared<Signal>(name, "", SignalType::INTEGER);
			}
			break;

		case MappingEntry::Type::TIMESTAMP:
			switch (index) {
				case 0:
					return std::make_shared<Signal>(name, "s", SignalType::INTEGER);
				case 1:
					return std::make_shared<Signal>(name, "ns", SignalType::INTEGER);
			}
			break;

		case MappingEntry::Type::DATA: {
			auto sig = std::make_shared<Signal>(data.signal->name, data.signal->unit, data.signal->type);

			sig->init = data.signal->init;

			return sig;
		}

		case MappingEntry::Type::UNKNOWN:
			break;
	}

	return std::shared_ptr<Signal>();
}
