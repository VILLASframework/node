/** Sample value remapping for mux.
 *
 * @file
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

#pragma once

#include <jansson.h>

#include <villas/stats.hpp>
#include <villas/common.hpp>

/* Forward declarations */
struct node;
struct sample;
struct signal;
struct vlist;

enum class MappingType {
	DATA,
	STATS,
	HEADER,
	TIMESTAMP
};

enum class MappingHeaderType {
	LENGTH,
	SEQUENCE
};

enum class MappingTimestampType {
	ORIGIN,
	RECEIVED
};

struct mapping_entry {
	struct node *node;		/**< The node to which this mapping refers. */

	enum MappingType type;		/**< The mapping type. Selects one of the union fields below. */

	/** The number of values which is covered by this mapping entry.
	 *
	 * A value of 0 indicates that all remaining values starting from the offset of a sample should be mapped.
	 */
	int length;
	unsigned offset;			/**< Offset of this mapping entry within sample::data */

	union {
		struct {
			int offset;
			struct signal *signal;
		} data;

		struct {
			enum villas::Stats::Metric metric;
			enum villas::Stats::Type type;
		} stats;

		struct {
			enum MappingHeaderType type;
		} header;

		struct {
			enum MappingTimestampType type;
		} timestamp;
	};
};

int mapping_update(const struct mapping_entry *e, struct sample *remapped, const struct sample *original);

int mapping_parse(struct mapping_entry *e, json_t *cfg, struct vlist *nodes);

int mapping_parse_str(struct mapping_entry *e, const char *str, struct vlist *nodes);

int mapping_to_str(const struct mapping_entry *me, unsigned index, char **str);

int mapping_list_parse(struct vlist *ml, json_t *cfg, struct vlist *nodes);

int mapping_list_prepare(struct vlist *ml);

int mapping_list_remap(const struct vlist *ml, struct sample *remapped, const struct sample *original);
