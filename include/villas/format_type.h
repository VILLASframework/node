/** Read / write sample data in different formats.
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

#include <stdio.h>

/* Forward declarations */
struct sample;
struct io;

enum format_type_flags {
	format_type_BINARY	= (1 << 8)
};

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
	void (*header)(struct io *io);

	/** Print a footer. */
	void (*footer)(struct io *io);

	/** @} */

	/** @{
	 * Low-level interface
	 */

	/** @see format_type_sscan */
	int (*sscan)(char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt, int flags);

	/** @see format_type_sprint */
	int (*sprint)(char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt, int flags);

	/** @see format_type_fscan */
	int (*fscan)(FILE *f, struct sample *smps[], unsigned cnt, int flags);

	/** @see format_type_fprint */
	int (*fprint)(FILE *f, struct sample *smps[], unsigned cnt, int flags);

	/** @} */

	size_t size;		/**< Number of bytes to allocate for io::_vd */
	int flags;		/**< A set of flags which is automatically used. */
};

struct format_type * format_type_lookup(const char *name);

/** Parse samples from the buffer \p buf with a length of \p len bytes.
 *
 * @param buf[in]	The buffer of data which should be parsed / de-serialized.
 * @param len[in]	The length of the buffer \p buf.
 * @param rbytes[out]	The number of bytes which have been read from \p buf.
 * @param smps[out]	The array of pointers to samples.
 * @param cnt[in]	The number of pointers in the array \p smps.
 *
 * @retval >=0		The number of samples which have been parsed from \p buf and written into \p smps.
 * @retval <0		Something went wrong.
 */
int format_type_sscan(struct format_type *fmt, char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt, int flags);

/** Print \p cnt samples from \p smps into buffer \p buf of length \p len.
 *
 * @param buf[out]	The buffer which should be filled with serialized data.
 * @param len[in]	The length of the buffer \p buf.
 * @param rbytes[out]	The number of bytes which have been written to \p buf. Ignored if NULL.
 * @param smps[in]	The array of pointers to samples.
 * @param cnt[in]	The number of pointers in the array \p smps.
 *
 * @retval >=0		The number of samples from \p smps which have been written into \p buf.
 * @retval <0		Something went wrong.
 */
int format_type_sprint(struct format_type *fmt, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt, int flags);

/** Parse up to \p cnt samples from stream \p f into array \p smps.
 *
 * @retval >=0		The number of samples which have been parsed from \p f and written into \p smps.
 * @retval <0		Something went wrong.
 */
int format_type_fscan(struct format_type *fmt, FILE *f, struct sample *smps[], unsigned cnt, int flags);

/** Print \p cnt samples from \p smps to stream \p f.
 *
 * @retval >=0		The number of samples from \p smps which have been written to \p f.
 * @retval <0		Something went wrong.
 */
int format_type_fprint(struct format_type *fmt, FILE *f, struct sample *smps[], unsigned cnt, int flags);
