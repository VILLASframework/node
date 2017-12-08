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

#include <villas/io.h>
#include <villas/io_format.h>
#include <villas/utils.h>
#include <villas/sample.h>

int io_init(struct io *io, struct io_format *fmt, int flags)
{
	io->_vt = fmt;
	io->_vd = alloc(fmt->size);

	io->flags = flags | io->_vt->flags;

	return io->_vt->init ? io->_vt->init(io) : 0;
}

int io_destroy(struct io *io)
{
	int ret;

	ret = io->_vt->destroy ? io->_vt->destroy(io) : 0;
	if (ret)
		return ret;

	free(io->_vd);

	return 0;
}

int io_stream_open(struct io *io, const char *uri)
{
	int ret;

	if (uri) {
		if (!strcmp(uri, "-")) {
			goto stdio;
		}
		else if (aislocal(uri)) {
			io->mode = IO_MODE_STDIO;

			io->stdio.output = fopen(uri, "a+");
			if (io->stdio.output == NULL)
				return -1;

			io->stdio.input  = fopen(uri, "r");
			if (io->stdio.input == NULL)
				return -1;
		}
		else {
			io->mode = IO_MODE_ADVIO;

			io->advio.output = afopen(uri, "a+");
			if (io->advio.output == NULL)
				return -1;

			io->advio.input = afopen(uri, "a+");
			if (io->advio.input == NULL)
				return -2;
		}
	}
	else {
stdio:		io->mode = IO_MODE_STDIO;
		io->flags |= IO_FLUSH;

		io->stdio.input  = stdin;
		io->stdio.output = stdout;
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
		ret = setvbuf(io->stdio.input, NULL, _IOLBF, BUFSIZ);
		if (ret)
			return -1;

		ret = setvbuf(io->stdio.output, NULL, _IOLBF, BUFSIZ);
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
			ret = afclose(io->advio.input);
			if (ret)
				return ret;

			ret = afclose(io->advio.output);
			if (ret)
				return ret;

			return 0;

		case IO_MODE_STDIO:
			if (io->stdio.input == stdin)
				return 0;

			ret = fclose(io->stdio.input);
			if (ret)
				return ret;

			ret = fclose(io->stdio.output);
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
			return afflush(io->advio.output);
		case IO_MODE_STDIO:
			return fflush(io->stdio.output);
		case IO_MODE_CUSTOM:
			return 0;
	}

	return -1;
}

int io_stream_eof(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			return afeof(io->advio.input);
		case IO_MODE_STDIO:
			return feof(io->stdio.input);
		case IO_MODE_CUSTOM:
			return 0;
	}

	return -1;
}

void io_stream_rewind(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			return arewind(io->advio.input);
		case IO_MODE_STDIO:
			return rewind(io->stdio.input);
		case IO_MODE_CUSTOM: { }
	}
}

int io_stream_fd(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			return afileno(io->advio.input);
		case IO_MODE_STDIO:
			return fileno(io->stdio.input);
		case IO_MODE_CUSTOM:
			return -1;
	}

	return -1;
}


int io_open(struct io *io, const char *uri)
{
	return io->_vt->open
		? io->_vt->open(io, uri)
		: io_stream_open(io, uri);
}

int io_close(struct io *io)
{
	return io->_vt->close
		? io->_vt->close(io)
	 	: io_stream_close(io);
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
	io->_vt->rewind
		? io->_vt->rewind(io)
		: io_stream_rewind(io);
}

int io_fd(struct io *io)
{
	return io->_vt->fd
		? io->_vt->fd(io)
		: io_stream_fd(io);
}

int io_print(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;

	if (io->_vt->print)
		ret = io->_vt->print(io, smps, cnt);
	else {
		FILE *f = io->mode == IO_MODE_ADVIO
				? io->advio.output->file
				: io->stdio.output;

		//flockfile(f);

		if (io->_vt->fprint)
			ret = io->_vt->fprint(f, smps, cnt, io->flags);
		else if (io->_vt->sprint) {
			char buf[4096];
			size_t wbytes;

			ret = io->_vt->sprint(buf, sizeof(buf), &wbytes, smps, cnt, io->flags);

			fwrite(buf, wbytes, 1, f);
		}
		else
			ret = -1;

		//funlockfile(f);
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
	else {
		FILE *f = io->mode == IO_MODE_ADVIO
				? io->advio.input->file
				: io->stdio.input;

		//flockfile(f);

		if (io->_vt->fscan)
			return io->_vt->fscan(f, smps, cnt, io->flags);
		else if (io->_vt->sscan) {
			size_t bytes, rbytes;
			char buf[4096];

			bytes = fread(buf, 1, sizeof(buf), f);

			ret = io->_vt->sscan(buf, bytes, &rbytes, smps, cnt, io->flags);
		}
		else
			ret = -1;

		//funlockfile(f);
	}

	return ret;
}
