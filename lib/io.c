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

static int io_print_lines(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret, i;

	FILE *f = io_stream_output(io);

	for (i = 0; i < cnt; i++) {
		size_t wbytes;

		ret = io_sprint(io, io->out.buffer, io->out.buflen, &wbytes, &smps[i], 1);
		if (ret < 0)
			return ret;

		fwrite(io->out.buffer, wbytes, 1, f);
	}

	return i;
}

static int io_scan_lines(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret, i;

	FILE *f = io_stream_input(io);

	for (i = 0; i < cnt; i++) {
		size_t rbytes;
		ssize_t bytes;
		char *ptr;

skip:		bytes = getdelim(&io->in.buffer, &io->in.buflen, io->delimiter, f);
		if (bytes < 0)
			return -1; /* An error or eof occured */

		/* Skip whitespaces, empty and comment lines */
		for (ptr = io->in.buffer; isspace(*ptr); ptr++);

		if (ptr[0] == '\0' || ptr[0] == '#')
			goto skip;

		ret = io_sscan(io, io->in.buffer, bytes, &rbytes, &smps[i], 1);
		if (ret < 0)
			return ret;
	}

	return i;
}

int io_init(struct io *io, struct format_type *fmt, struct node *n, int flags)
{
	int ret;

	assert(io->state == STATE_DESTROYED);

	io->_vt = fmt;
	io->_vd = alloc(fmt->size);

	io->flags = flags | io->_vt->flags;
	io->delimiter = io->_vt->delimiter ? io->_vt->delimiter : '\n';
	io->separator = io->_vt->separator ? io->_vt->separator : '\t';

	io->in.buflen =
	io->out.buflen = 4096;

	io->in.buffer = alloc(io->in.buflen);
	io->out.buffer = alloc(io->out.buflen);

	io->input.node = n;
	io->output.node = n;

	if (n) {
		io->input.signals = &n->in.signals;
		io->output.signals = &n->out.signals;
	}
	else {
		io->input.signals = NULL;
		io->output.signals = NULL;
	}

	ret = io->_vt->init ? io->_vt->init(io) : 0;
	if (ret)
		return ret;

	io->state = STATE_INITIALIZED;

	return 0;
}

int io_destroy(struct io *io)
{
	int ret;

	assert(io->state == STATE_CLOSED || io->state == STATE_INITIALIZED);

	ret = io->_vt->destroy ? io->_vt->destroy(io) : 0;
	if (ret)
		return ret;

	free(io->_vd);
	free(io->in.buffer);
	free(io->out.buffer);

	io->state = STATE_DESTROYED;

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

			io->out.stream.std = fopen(uri, "a+");
			if (io->out.stream.std == NULL)
				return -1;

			io->in.stream.std  = fopen(uri, "r");
			if (io->in.stream.std == NULL)
				return -1;
		}
		else {
			io->mode = IO_MODE_ADVIO;

			io->out.stream.adv = afopen(uri, "a+");
			if (io->out.stream.adv == NULL)
				return -1;

			io->in.stream.adv = afopen(uri, "a+");
			if (io->in.stream.adv == NULL)
				return -2;
		}
	}
	else {
stdio:		io->mode = IO_MODE_STDIO;
		io->flags |= IO_FLUSH;

		io->in.stream.std  = stdin;
		io->out.stream.std = stdout;
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
		ret = setvbuf(io->in.stream.std, NULL, _IOLBF, BUFSIZ);
		if (ret)
			return -1;

		ret = setvbuf(io->out.stream.std, NULL, _IOLBF, BUFSIZ);
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
			ret = afclose(io->in.stream.adv);
			if (ret)
				return ret;

			ret = afclose(io->out.stream.adv);
			if (ret)
				return ret;

			return 0;

		case IO_MODE_STDIO:
			if (io->in.stream.std == stdin)
				return 0;

			ret = fclose(io->in.stream.std);
			if (ret)
				return ret;

			ret = fclose(io->out.stream.std);
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
			return afflush(io->out.stream.adv);
		case IO_MODE_STDIO:
			return fflush(io->out.stream.std);
		case IO_MODE_CUSTOM:
			return 0;
	}

	return -1;
}

int io_stream_eof(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			return afeof(io->in.stream.adv);
		case IO_MODE_STDIO:
			return feof(io->in.stream.std);
		case IO_MODE_CUSTOM:
			return 0;
	}

	return -1;
}

void io_stream_rewind(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			arewind(io->in.stream.adv);
			break;
		case IO_MODE_STDIO:
			rewind(io->in.stream.std);
			break;
		case IO_MODE_CUSTOM: { }
	}
}

int io_stream_fd(struct io *io)
{
	switch (io->mode) {
		case IO_MODE_ADVIO:
			return afileno(io->in.stream.adv);
		case IO_MODE_STDIO:
			return fileno(io->in.stream.std);
		case IO_MODE_CUSTOM:
			return -1;
	}

	return -1;
}

int io_open(struct io *io, const char *uri)
{
	int ret;

	assert(io->state == STATE_INITIALIZED);

	ret = io->_vt->open
		? io->_vt->open(io, uri)
		: io_stream_open(io, uri);
	if (ret)
		return ret;

	io->state = STATE_OPENED;

	io_header(io);

	return 0;
}

int io_close(struct io *io)
{
	int ret;

	assert(io->state == STATE_OPENED);

	io_footer(io);

	ret = io->_vt->close
		? io->_vt->close(io)
	 	: io_stream_close(io);
	if (ret)
		return ret;

	io->state = STATE_CLOSED;

	return 0;
}

int io_flush(struct io *io)
{
	assert(io->state == STATE_OPENED);

	return io->_vt->flush
		? io->_vt->flush(io)
		: io_stream_flush(io);
}

int io_eof(struct io *io)
{
	assert(io->state == STATE_OPENED);

	return io->_vt->eof
		? io->_vt->eof(io)
		: io_stream_eof(io);
}

void io_rewind(struct io *io)
{
	assert(io->state == STATE_OPENED);

	if (io->_vt->rewind)
		io->_vt->rewind(io);
	else
		io_stream_rewind(io);
}

int io_fd(struct io *io)
{
	assert(io->state == STATE_OPENED);

	return io->_vt->fd
		? io->_vt->fd(io)
		: io_stream_fd(io);
}

void io_header(struct io *io)
{
	assert(io->state == STATE_OPENED);

	if (io->_vt->header)
		io->_vt->header(io);
}

void io_footer(struct io *io)
{
	assert(io->state == STATE_OPENED);

	if (io->_vt->footer)
		io->_vt->footer(io);
}

int io_print(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;

	assert(io->state == STATE_OPENED);

	if (io->flags & IO_NEWLINES)
		ret = io_print_lines(io, smps, cnt);
	else if (io->_vt->print)
		ret = io->_vt->print(io, smps, cnt);
	else if (io->_vt->sprint) {
		FILE *f = io_stream_output(io);
		size_t wbytes;

		ret = io_sprint(io, io->out.buffer, io->out.buflen, &wbytes, smps, cnt);

		fwrite(io->out.buffer, wbytes, 1, f);
	}
	else
		ret = -1;

	if (io->flags & IO_FLUSH)
		io_flush(io);

	return ret;
}

int io_scan(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;

	assert(io->state == STATE_OPENED);

	if (io->flags & IO_NEWLINES)
		ret = io_scan_lines(io, smps, cnt);
	else if (io->_vt->scan)
		ret = io->_vt->scan(io, smps, cnt);
	else if (io->_vt->sscan) {
		FILE *f = io_stream_input(io);
		size_t bytes, rbytes;

		bytes = fread(io->in.buffer, 1, io->in.buflen, f);

		ret = io_sscan(io, io->in.buffer, bytes, &rbytes, smps, cnt);
	}
	else
		ret = -1;

	return ret;
}

FILE * io_stream_output(struct io *io) {
	if (io->state != STATE_OPENED)
		return 0;

	return io->mode == IO_MODE_ADVIO
		? io->out.stream.adv->file
		: io->out.stream.std;
}

FILE * io_stream_input(struct io *io) {
	if (io->state != STATE_OPENED)
		return 0;

	return io->mode == IO_MODE_ADVIO
		? io->in.stream.adv->file
		: io->in.stream.std;
}

int io_sscan(struct io *io, char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	struct format_type *fmt = io->_vt;

	return fmt->sscan ? fmt->sscan(io, buf, len, rbytes, smps, cnt) : -1;
}

int io_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	struct format_type *fmt = io->_vt;

	return fmt->sprint ? fmt->sprint(io, buf, len, wbytes, smps, cnt) : -1;
}
