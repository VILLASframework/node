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
struct format_type;

enum io_flags {
	IO_FLUSH		= (1 << 8),	/**< Flush the output stream after each chunk of samples. */
	IO_NONBLOCK		= (1 << 9),	/**< Dont block io_read() while waiting for new samples. */
	IO_NEWLINES		= (1 << 10)	/**< The samples of this format are newline delimited. */
};

struct io {
	enum state state;
	int flags;
	char delimiter;				/**< Newline delimiter. */

	struct {
		/** A format type can use this file handle or overwrite the
		 * format::{open,close,eof,rewind} functions and the private
		 * data in io::_vd.
		 */
		union {
			FILE *std;
			AFILE *adv;
		} stream;

		char *buffer;
		size_t buflen;

		struct list *signals;
		struct node *node;
	} input, output;

	enum {
		IO_MODE_STDIO,
		IO_MODE_ADVIO,
		IO_MODE_CUSTOM
	} mode;

	void *_vd;
	struct format_type *_vt;
};

int io_init(struct io *io, struct format_type *fmt, int flags);

int io_destroy(struct io *io);

int io_open(struct io *io, const char *uri);

int io_close(struct io *io);

void io_header(struct io *io);

void io_footer(struct io *io);

int io_print(struct io *io, struct sample *smps[], unsigned cnt);

int io_scan(struct io *io, struct sample *smps[], unsigned cnt);

int io_scan_lines(struct io *io, struct sample *smps[], unsigned cnt);

int io_print_lines(struct io *io, struct sample *smps[], unsigned cnt);

int io_eof(struct io *io);

void io_rewind(struct io *io);

int io_flush(struct io *io);

int io_fd(struct io *io);

int io_stream_open(struct io *io, const char *uri);

int io_stream_close(struct io *io);

int io_stream_eof(struct io *io);

void io_stream_rewind(struct io *io);

int io_stream_fd(struct io *io);

int io_stream_flush(struct io *io);

FILE * io_stream_input(struct io *io);
FILE * io_stream_output(struct io *io);
