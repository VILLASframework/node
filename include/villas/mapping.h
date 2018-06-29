/** Sample value remapping for mux.
 *
 * @file
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

#pragma once

#include <jansson.h>

#include <villas/stats.h>
#include <villas/common.h>
#include <villas/list.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct stats;
struct node;
struct sample;
struct list;

struct mapping_entry {
	struct node *node;

	int offset;			/**< Offset of this mapping entry within sample::data */
	int length;			/**< The number of values which is covered by this mapping entry. */

	enum {
		MAPPING_TYPE_DATA,
		MAPPING_TYPE_STATS,
		MAPPING_TYPE_HDR,
		MAPPING_TYPE_TS
	} type;

	union {
		struct {
			int offset;
		} data;

		struct {
			enum stats_id id;
			enum stats_type {
				MAPPING_STATS_TYPE_LAST,
				MAPPING_STATS_TYPE_HIGHEST,
				MAPPING_STATS_TYPE_LOWEST,
				MAPPING_STATS_TYPE_MEAN,
				MAPPING_STATS_TYPE_VAR,
				MAPPING_STATS_TYPE_STDDEV,
				MAPPING_STATS_TYPE_TOTAL
			} type;
		} stats;

		struct {
			enum header_type {
				MAPPING_HDR_LENGTH,
				MAPPING_HDR_SEQUENCE,
				MAPPING_HDR_FORMAT
			} id;
		} hdr;

		struct {
			enum timestamp_type {
				MAPPING_TS_ORIGIN,
				MAPPING_TS_RECEIVED,
				MAPPING_TS_SEND
			} id;
		} ts;
	};
};

int mapping_remap(struct list *m, struct sample *remapped, struct sample *original, struct stats *s);

int mapping_update(struct mapping_entry *e, struct sample *remapped, struct sample *new, struct stats *s);

int mapping_parse(struct mapping_entry *e, json_t *cfg, struct list *nodes);

int mapping_parse_str(struct mapping_entry *e, const char *str, struct list *nodes);

int mapping_parse_list(struct list *l, json_t *cfg, struct list *nodes);

#ifdef __cplusplus
}
#endif
