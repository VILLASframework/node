/** JSON serializtion sample data.
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

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct sample;

int json_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt);
int json_sscan(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt);

int json_print(struct io *io, struct sample *smps[], unsigned cnt);
int json_scan(struct io *io, struct sample *smps[], unsigned cnt);

#ifdef __cplusplus
}
#endif
