/** RAW IO format
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

#include <stdlib.h>

/* Forward declarations */
struct sample;

enum raw_flags {
	RAW_FAKE	= (1 << 16), /**< Treat the first three values as: sequenceno, seconds, nanoseconds */

	RAW_BE_INT	= (1 << 17), /**< Byte-order for integer data: big-endian if set. */
	RAW_BE_FLT	= (1 << 18), /**< Byte-order for floating point data: big-endian if set. */
	RAW_BE_HDR	= (1 << 19), /**< Byte-order for fake header fields: big-endian if set. */

	/** Byte-order for all fields: big-endian if set. */
	RAW_BE		= RAW_BE_INT | RAW_BE_FLT | RAW_BE_HDR,

	/** Mix floating and integer types.
	 *
	 * io_raw_sscan()  parses all values as single / double precission fp.
	 * io_raw_sprint() uses sample::format to determine the type.
	 */
	RAW_AUTO	= (1 << 22),
	RAW_FLT		= (1 << 23), /**< Data-type: floating point otherwise integer. */

	//RAW_1	= (0 << 24), /**< Pack each value as a single bit. */
	RAW_8		= (3 << 24), /**< Pack each value as a byte. */
	RAW_16		= (4 << 24), /**< Pack each value as a word. */
	RAW_32		= (5 << 24), /**< Pack each value as a double word. */
	RAW_64		= (6 << 24)  /**< Pack each value as a quad word. */
};

/** Copy / read struct msg's from buffer \p buf to / fram samples \p smps. */
int raw_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt);

/** Read struct sample's from buffer \p buf into samples \p smps. */
int raw_sscan(struct io *io, char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt);
