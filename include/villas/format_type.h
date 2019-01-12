/** Read / write sample data in different formats.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct sample;
struct io;

struct format_type {
	int (*init)(struct io *io);
	int (*destroy)(struct io *io);

	/** @{
	 * High-level interface
	 */

	/** Open an IO stream.
	 *
	 * @see fopen()
	 */
	int (*open)(struct io *io, const char *uri);

	/** Close an IO stream.
	 *
	 * @see fclose()
	 */
	int (*close)(struct io *io);

	/** Check if end-of-file was reached.
	 *
	 * @see feof()
	 */
	int (*eof)(struct io *io);

	/** Rewind an IO stream.
	 *
	 * @see rewind()
	 */
	void (*rewind)(struct io *io);

	/** Get a file descriptor which can be used with select / poll */
	int (*fd)(struct io *io);

	/** Flush buffered data to disk.
	 *
	 * @see fflush()
	 */
	int (*flush)(struct io *io);

	int (*print)(struct io *io, struct sample *smps[], unsigned cnt);
	int (*scan)( struct io *io, struct sample *smps[], unsigned cnt);

	/** Print a header. */
	void (*header)(struct io *io, const struct sample *smp);

	/** Print a footer. */
	void (*footer)(struct io *io);

	/** @} */

	/** @{
	 * Low-level interface
	 */

	/** @see format_type_sscan */
	int (*sscan)(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt);

	/** @see format_type_sprint */
	int (*sprint)(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt);

	/** @} */

	size_t size;				/**< Number of bytes to allocate for io::_vd */
	int flags;				/**< A set of flags which is automatically used. */
	char delimiter;				/**< Newline delimiter. */
	char separator;				/**< Column separator (used by csv and villas.human formats only) */
};

struct format_type * format_type_lookup(const char *name);

const char * format_type_name(struct format_type *vt);

#ifdef __cplusplus
}
#endif
