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

#include <libconfig.h>

#include "stats.h"
#include "common.h"
#include "list.h"

/* Forward declarations */
struct stats;
struct node;
struct sample;
struct list;

struct mapping_entry {
	struct node *source; /**< Unused for now. */
	int length;

	enum {
		MAPPING_TYPE_DATA,
		MAPPING_TYPE_STATS,
		MAPPING_TYPE_HEADER,
		MAPPING_TYPE_TIMESTAMP
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
				MAPPING_HEADER_LENGTH,
				MAPPING_HEADER_SEQUENCE,
			} id;
		} header;
		struct {
			enum timestamp_type {
				MAPPING_TIMESTAMP_ORIGIN,
				MAPPING_TIMESTAMP_RECEIVED
			} id;
		} timestamp;
	};
};

struct mapping {
	enum state state;

	int real_length;

	struct list entries;
};

int mapping_init(struct mapping *m);

int mapping_parse(struct mapping *m, config_setting_t *cfg);

int mapping_check(struct mapping *m);

int mapping_destroy(struct mapping *m);

int mapping_remap(struct mapping *m, struct sample *orig, struct sample *remapped, struct stats *s);

int mapping_entry_parse(struct mapping_entry *e, config_setting_t *cfg);

int mapping_entry_parse_str(struct mapping_entry *e, const char *str);