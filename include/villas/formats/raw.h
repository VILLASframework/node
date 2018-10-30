/** RAW IO format
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

/* float128 is currently not yet supported as htole128() functions a missing */
#if 0 && defined(__GNUC__) && defined(__linux__)
  #define HAS_128BIT
#endif

/* Forward declarations */
struct sample;

enum raw_flags {
	/** Treat the first three values as: sequenceno, seconds, nanoseconds */
	RAW_FAKE_HEADER	= (1 << 16),
	RAW_BIG_ENDIAN	= (1 << 7),	/**< Encode data in big-endian byte order */

	RAW_BITS_8	= (3 << 24),	/**< Pack each value as a byte. */
	RAW_BITS_16	= (4 << 24),	/**< Pack each value as a word. */
	RAW_BITS_32	= (5 << 24),	/**< Pack each value as a double word. */
	RAW_BITS_64	= (6 << 24), 	/**< Pack each value as a quad word. */
#ifdef HAS_128BIT
	RAW_128		= (7 << 24)  /**< Pack each value as a double quad word. */
#endif
};

/** Copy / read struct msg's from buffer \p buf to / fram samples \p smps. */
int raw_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt);

/** Read struct sample's from buffer \p buf into samples \p smps. */
int raw_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt);

#ifdef __cplusplus
}
#endif
