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

#include "advio.h"
#include "common.h"

/* Forward declarations */
struct sample;
struct io;

/** These flags define the format which is used by io_fscan() and io_fprint(). */
enum io_flags {
	IO_FORMAT_NANOSECONDS	= (1 << 0), /**< Include nanoseconds in output. */
	IO_FORMAT_OFFSET	= (1 << 1), /**< Include offset / delta between received and send timestamps. */
	IO_FORMAT_SEQUENCE	= (1 << 2), /**< Include sequence number in output. */
	IO_FORMAT_VALUES	= (1 << 3), /**< Include values in output. */
	IO_FORMAT_ALL		= 16-1,     /**< Enable all output options. */
	IO_FLAG_FLUSH		= (1 << 8), /**< Flush the output stream after each chunk of samples. */
};

struct io_format {
	int (*init)(struct io *io);
	int (*destroy)(struct io *io);

	/** @{
	 * High-level interface
	 */

	/** Open an IO stream.
	 *
	 * @see fopen()
	 */
	int (*open)(struct io *io, const char *uri, const char *mode);

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

	/** Flush buffered data to disk.
	 *
	 * @see fflush()
	 */
	int (*flush)(struct io *io);

	int (*print)(struct io *io, struct sample *smps[], size_t cnt);
	int (*scan)( struct io *io, struct sample *smps[], size_t cnt);
	/** @} */

	/** @{
	 * Low-level interface
	 */

	/** Parse samples from the buffer \p buf with a length of \p len bytes.
	 *
	 * @return The number of bytes consumed of \p buf.
	 */
	size_t (*sscan)( char *buf, size_t len, struct sample *smps[], size_t cnt, int *flags);

	/** Print \p cnt samples from \p smps into buffer \p buf of length \p len.
	 *
	 * @return The number of bytes written to \p buf.
	 */
	size_t (*sprint)(char *buf, size_t len, struct sample *smps[], size_t cnt, int flags);

	/** Parse up to \p cnt samples from stream \p f into array \p smps.
	 *
	 * @return The number of samples parsed.
	 */
	int (*fscan)( FILE *f, struct sample *smps[], size_t cnt, int *flags);

	/** Print \p cnt samples from \p smps to stream \p f.
	 *
	 * @return The number of samples written to \p f.
	 */
	int (*fprint)(FILE *f, struct sample *smps[], size_t cnt, int  flags);

	/** @} */

	size_t size; /**< Number of bytes to allocate for io::_vd */
};

struct io {
	enum state state;
	int flags;

	enum {
		IO_MODE_STDIO,
		IO_MODE_ADVIO,
		IO_MODE_CUSTOM
	} mode;

	/** A format type can use this file handle or overwrite the
	 * io_format::{open,close,eof,rewind} functions and the private
	 * data in io::_vd.
	 */
	union {
		struct {
			FILE *input;
			FILE *output;
		} stdio;
		struct {
			AFILE *input;
			AFILE *output;
		} advio;
	};

	void *_vd;
	struct io_format *_vt;
};

int io_init(struct io *io, struct io_format *fmt, int flags);

int io_destroy(struct io *io);

int io_open(struct io *io, const char *uri, const char *mode);

int io_close(struct io *io);

int io_print(struct io *io, struct sample *smps[], size_t cnt);

int io_scan(struct io *io, struct sample *smps[], size_t cnt);

int io_eof(struct io *io);

void io_rewind(struct io *io);

int io_flush(struct io *io);


int io_stream_open(struct io *io, const char *uri, const char *mode);

int io_stream_close(struct io *io);

int io_stream_eof(struct io *io);

void io_stream_rewind(struct io *io);

int io_stream_flush(struct io *io);
