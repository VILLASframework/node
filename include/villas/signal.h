/** Signal meta data.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
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

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct list;
struct node;
struct mapping_entry;

enum signal_format {
	SIGNAL_FORMAT_FLOAT	= 0,
	SIGNAL_FORMAT_INT	= 1,
	SIGNAL_FORMAT_BOOL 	= 2,
	SIGNAL_FORMAT_COMPLEX 	= 3,
	SIGNAL_FORMAT_UNKNOWN	= -1
};

struct signal {
	char *name;	/**< The name of the signal. */
	char *unit;
	int enabled;
	enum signal_format format;
};

int signal_init(struct signal *s);

int signal_init_from_mapping(struct signal *s, const struct mapping_entry *me, unsigned index);

int signal_destroy(struct signal *s);

int signal_parse(struct signal *s, json_t *cfg);

int signal_parse_list(struct list *list, json_t *cfg);

int signal_get_offset(const char *str, struct node *n);

#ifdef __cplusplus
}
#endif
