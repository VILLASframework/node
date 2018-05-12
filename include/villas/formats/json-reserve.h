/** JSON serializtion for RESERVE project.
 *
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

/* Forward declarations */
struct sample;

typedef int (*json_pack_cb_t)(json_t **j, struct sample *smp, int flags);
typedef int (*json_unpack_cb_t)(json_t *j, struct sample *s, int flags);

int json_reserve_pack_sample(json_t **j, struct sample *s, int flags);
int json_reserve_unpack_sample(json_t *j, struct sample *s, int flags);

int json_reserve_fprint(FILE *f, struct sample *smps[], unsigned cnt, int flags);
int json_reserve_fscan(FILE *f, struct sample *smps[], unsigned cnt, int flags);
