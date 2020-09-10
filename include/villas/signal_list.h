/** Signal metadata lits.
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

#include <villas/signal_type.h>

/* Forward declarations */
struct vlist;

int signal_list_init(struct vlist *list) __attribute__ ((warn_unused_result));

int signal_list_destroy(struct vlist *list) __attribute__ ((warn_unused_result));

int signal_list_parse(struct vlist *list, json_t *cfg);

int signal_list_generate(struct vlist *list, unsigned len, enum SignalType fmt);

int signal_list_generate2(struct vlist *list, const char *dt);

void signal_list_dump(const struct vlist *list, const union signal_data *data = nullptr, unsigned len = 0);

int signal_list_copy(struct vlist *dst, const struct vlist *src);

json_t * signal_list_to_json(struct vlist *list);
