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

struct signal {
	char *name;	/**< The name of the signal. */
	char *unit;
	int enabled;
	enum {
		SIGNAL_FORMAT_INTEGER,
		SIGNAL_FORMAT_REAL
	} format;
};

int signal_destroy(struct signal *s);

int signal_parse(struct signal *s, json_t *cfg);

int signal_parse_list(struct list *list, json_t *cfg);

int signal_get_offset(const char *str, struct node *n);

#ifdef __cplusplus
}
#endif
