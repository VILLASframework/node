/** Reading and writing simulation samples in various formats.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include <villas/io.h>
#include <villas/format_type.h>
#include <villas/utils.h>
#include <villas/sample.h>

int io_init(struct io *io, struct format_type *fmt, int flags)
{
	io->_vt = fmt;
	io->_vd = alloc(fmt->size);

	io->flags = flags | io->_vt->flags;
	io->delimiter = '\n';

	io->input.buflen =
	io->output.buflen = 4096;

	io->input.buffer = alloc(io->input.buflen);
	io->output.buffer = alloc(io->output.buflen);

	return io->_vt->init ? io->_vt->init(io) : 0;
}

int io_destroy(struct io *io)
{
	int ret;

	ret = io->_vt->destroy ? io->_vt->destroy(io) : 0;
	if (ret)
		return ret;

	free(io->_vd);
	free(io->input.buffer);
	free(io->output.buffer);

	return 0;
}

int io_stream_open(struct io *io, const char *uri)
{
	int ret;

	if (uri) {
		if (!strcmp(uri, "-")) {
			goto stdio;
		}
		else if (aislocal(uri) == 1) {
			io->mode = IO_MODE_STDIO;

			io->output.stream.std = fopen(uri, "a+");
			if (io->output.stream.std == NULL)
				return -1;

			io->input.stream.std  = fopen(uri, "r");
			if (io->input.stream.std == NULL)
				return -1;
		}
		else {
			io->mode = IO_MODE_ADVIO;

			io->output.stream.adv = afopen(uri, "a+");
			if (io->output.stream.adv == NULL)
				return -1;

			io->input.stream.adv = afopen(uri, "a+");
			if (io->input.stream.adv == NULL)
				return -2;
		}
	}
	else {
stdio:		io->mode = IO_MODE_STDIO;
		io->flags |= IO_FLUSH;

		io->input.stream.std  = stdin;
		io->output.stream.std = stdout;
	}

	/* Make stream non-blocking if desired */
	if (io->flags & IO_NONBLOCK) {
		int ret, fd, flags;

		fd = io_fd(io);
		if (fd < 0)
			return fd;

		flags = fcntl(fd, F_GETFL);
		if (flags < 0)
			return flags;

		flags |= O_NONBLOCK;

		ret = fcntl(fd, F_SETFL, flags);
		if (ret)
			return ret;
	}

	/* Enable line buffering on stdio */
	if (io->mode == IO_MODE_STDIO) {
		ret = setvbuf(io->input.stream.std, NULL, _IOLBF, BUFSIZ);
		if (ret)
			return -1;

		ret = setvbuf(io->output.stream.std, NULL, _IOLBF, BUFSIZ);
		if (ret)
			return -1;
	}

	return 0;
}

int io_stream_close(struct io *io)
{
	int ret;

	switch (io->mode) {
		case IO_MODE_ADVIO:
			ret = afclose(io->input.stream.adv);
			if (ret)
				return ret;

			ret = afclose(io->output.stream.adv);
			if (ret)
				return ret;

			return 0;

		case IO_MODE_STDIO:
			if (io->input.stream.std == stdin)
				return 0;

			ret = fclose(io->input.stream.std);
			if (ret)
				return ret;

			ret = fclose(io->output.stream.std);
			if (ret)
				return ret;

			return 0;

		case IO_MODE_CUSTOM:
			return 0;
	}

	return -1;
}

int io_stream_flush(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			return afflush(io->output.stream.adv);
		case IO_MODE_STDIO:
			return fflush(io->output.stream.std);
		case IO_MODE_CUSTOM:
			return 0;
	}

	return -1;
}

int io_stream_eof(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			return afeof(io->input.stream.adv);
		case IO_MODE_STDIO:
			return feof(io->input.stream.std);
		case IO_MODE_CUSTOM:
			return 0;
	}

	return -1;
}

void io_stream_rewind(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			arewind(io->input.stream.adv);
			break;
		case IO_MODE_STDIO:
			rewind(io->input.stream.std);
			break;
		case IO_MODE_CUSTOM: { }
	}
}

int io_stream_fd(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			return afileno(io->input.stream.adv);
		case IO_MODE_STDIO:
			return fileno(io->input.stream.std);
		case IO_MODE_CUSTOM:
			return -1;
	}

	return -1;
}

int io_open(struct io *io, const char *uri)
{
	int ret;

	ret = io->_vt->open
		? io->_vt->open(io, uri)
		: io_stream_open(io, uri);
	if (ret)
		return ret;

	io_header(io);

	return ret;
}

int io_close(struct io *io)
{
	int ret;

	io_footer(io);

	ret = io->_vt->close
		? io->_vt->close(io)
	 	: io_stream_close(io);

	return ret;
}

int io_flush(struct io *io)
{
	return io->_vt->flush
		? io->_vt->flush(io)
		: io_stream_flush(io);
}

int io_eof(struct io *io)
{
	return io->_vt->eof
		? io->_vt->eof(io)
		: io_stream_eof(io);
}

void io_rewind(struct io *io)
{
	if (io->_vt->rewind)
		io->_vt->rewind(io);
	else
		io_stream_rewind(io);
}

int io_fd(struct io *io)
{
	return io->_vt->fd
		? io->_vt->fd(io)
		: io_stream_fd(io);
}

void io_header(struct io *io)
{
	if (io->_vt->header)
		io->_vt->header(io);
}

void io_footer(struct io *io)
{
	if (io->_vt->footer)
		io->_vt->footer(io);
}

int io_print(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;

	if (io->_vt->print)
		ret = io->_vt->print(io, smps, cnt);
	else if (io->flags & IO_NEWLINES)
		ret = io_print_lines(io, smps, cnt);
	else {
		FILE *f = io_stream_output(io);

		if (io->_vt->fprint)
			ret = format_type_fprint(io->_vt, f, smps, cnt, io->flags);
		else if (io->_vt->sprint) {
			size_t wbytes;

			ret = format_type_sprint(io->_vt, io->output.buffer, io->output.buflen, &wbytes, smps, cnt, io->flags);

			fwrite(io->output.buffer, wbytes, 1, f);
		}
		else
			ret = -1;
	}

	if (io->flags & IO_FLUSH)
		io_flush(io);

	return ret;
}

int io_scan(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;

	if (io->_vt->scan)
		ret = io->_vt->scan(io, smps, cnt);
	else if (io->flags & IO_NEWLINES)
		ret = io_scan_lines(io, smps, cnt);
	else {
		FILE *f = io_stream_input(io);

		if (io->_vt->fscan)
			ret = format_type_fscan(io->_vt, f, smps, cnt, io->flags);
		else if (io->_vt->sscan) {
			size_t bytes, rbytes;

			bytes = fread(io->input.buffer, 1, io->input.buflen, f);

			ret = format_type_sscan(io->_vt, io->input.buffer, bytes, &rbytes, smps, cnt, io->flags);
		}
		else
			ret = -1;
	}

	return ret;
}

int io_print_lines(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret, i;

	FILE *f = io_stream_output(io);

	for (i = 0; i < cnt; i++) {
		size_t wbytes;

		ret = format_type_sprint(io->_vt, io->output.buffer, io->output.buflen, &wbytes, &smps[i], 1, io->flags);
		if (ret < 0)
			return ret;

		fwrite(io->output.buffer, wbytes, 1, f);
	}

	return i;
}

int io_scan_lines(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret, i;

	FILE *f = io_stream_input(io);

	for (i = 0; i < cnt; i++) {
		size_t rbytes;
		ssize_t bytes;
		char *ptr;

skip:		bytes = getdelim(&io->input.buffer, &io->input.buflen, io->delimiter, f);
		if (bytes < 0)
			return -1; /* An error or eof occured */

		/* Skip whitespaces, empty and comment lines */
		for (ptr = io->input.buffer; isspace(*ptr); ptr++);

		if (ptr[0] == '\0' || ptr[0] == '#')
			goto skip;

		ret = format_type_sscan(io->_vt, io->input.buffer, bytes, &rbytes, &smps[i], 1, io->flags);
		if (ret < 0)
			return ret;
	}

	return i;
}

FILE * io_stream_output(struct io *io) {
	return io->mode == IO_MODE_ADVIO
		? io->output.stream.adv->file
		: io->output.stream.std;
}

FILE * io_stream_input(struct io *io) {
	return io->mode == IO_MODE_ADVIO
		? io->input.stream.adv->file
		: io->input.stream.std;
}
