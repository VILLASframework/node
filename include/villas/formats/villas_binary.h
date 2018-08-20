/** Message related functions
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations. */
struct sample;
struct msg;
struct io;

enum villas_binary_flags {
	VILLAS_BINARY_WEB	= (1 << 16) /**< Use webmsg format (everying little endian) */
};

/** Copy / read struct msg's from buffer \p buf to / fram samples \p smps. */
int villas_binary_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt);

/** Read struct sample's from buffer \p buf into samples \p smps. */
int villas_binary_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt);

#ifdef __cplusplus
}
#endif
