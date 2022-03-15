/** Sample value remapping for path source muxing.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <jansson.h>
#include <memory>

#include <villas/common.hpp>
#include <villas/stats.hpp>
#include <villas/node_list.hpp>

#define RE_MAPPING_INDEX "[a-zA-Z0-9_]+"
#define RE_MAPPING_RANGE "(" RE_MAPPING_INDEX ")(?:-(" RE_MAPPING_INDEX "))?"

#define RE_MAPPING_STATS "stats\\.([a-z]+)\\.([a-z]+)"
#define RE_MAPPING_HDR   "hdr\\.(sequence|length)"
#define RE_MAPPING_TS    "ts\\.(origin|received)"
#define RE_MAPPING_DATA1 "data\\[" RE_MAPPING_RANGE "\\]"
#define RE_MAPPING_DATA2 "(?:data\\.)?(" RE_MAPPING_INDEX ")"
#define RE_MAPPING       "(?:(" RE_NODE_NAME ")\\.(?:" RE_MAPPING_STATS "|" RE_MAPPING_HDR "|" RE_MAPPING_TS "|" RE_MAPPING_DATA1 "|" RE_MAPPING_DATA2 ")|(" RE_NODE_NAME ")(?:\\[" RE_MAPPING_RANGE "\\])?)"

namespace villas {
namespace node {

/* Forward declarations */
class Node;
struct Sample;
class Signal;

class MappingEntry {

public:
	using Ptr = std::shared_ptr<MappingEntry>;

	enum class Type {
		UNKNOWN,
		DATA,
		STATS,
		HEADER,
		TIMESTAMP
	};

	enum class HeaderType {
		LENGTH,
		SEQUENCE
	};

	enum class TimestampType {
		ORIGIN,
		RECEIVED
	};

	Node *node;	/**< The node to which this mapping refers. */
	enum Type type;		/**< The mapping type. Selects one of the union fields below. */

	/** The number of values which is covered by this mapping entry.
	 *
	 * A value of 0 indicates that all remaining values starting from the offset of a sample should be mapped.
	 */
	int length;
	unsigned offset;		/**< Offset of this mapping entry within sample::data */

	union {
		struct {
			int offset;
			Signal *signal;

			char *first;
			char *last;
		} data;

		struct {
			enum Stats::Metric metric;
			enum Stats::Type type;
		} stats;

		struct {
			enum HeaderType type;
		} header;

		struct {
			enum TimestampType type;
		} timestamp;
	};

	std::string nodeName;	/**< Used for between parse and prepare only. */

	MappingEntry();

	int prepare(NodeList &nodes);

	int update(struct Sample *remapped, const struct Sample *original) const;

	int parse(json_t *json);

	int parseString(const std::string &str);

	std::string toString(unsigned index) const;

	Signal::Ptr toSignal(unsigned index) const;
};

} /* namespace node */
} /* namespace villas */
