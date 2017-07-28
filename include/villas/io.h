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

/* Forward declarations */
struct sample;
struct io;

/** These flags define the format which is used by io_fscan() and io_fprint(). */
enum io_flags {
	IO_FORMAT_NANOSECONDS	= (1 << 0),
	IO_FORMAT_OFFSET	= (1 << 1),
	IO_FORMAT_SEQUENCE	= (1 << 2),
	IO_FORMAT_VALUES	= (1 << 3),
	IO_FORMAT_ALL		= 16-1
};

struct io_format {
	int (*init)(struct io *io);
	int (*destroy)(struct io *io);
	int (*open)(struct io *io, const char *uri, const char *mode);
	int (*close)(struct io *io);
	int (*eof)(struct io *io);
	void (*rewind)(struct io *io);
	int (*print)(struct io *io, struct sample *s, int fl);
	int (*scan)(struct io *io, struct sample *s, int *fl);

	size_t size; /**< Number of bytes to allocate for io::_vd */
};

struct io {
	int flags;

	/** A format type can use this file handle or overwrite the
	 * io_format::{open,close,eof,rewind} functions and the private
	 * data in io::_vd.
	 */
	AFILE *file;

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
