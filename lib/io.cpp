/** Reading and writing simulation samples in various formats.
 *
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>

#include <villas/io.h>
#include <villas/format_type.h>
#include <villas/utils.hpp>
#include <villas/sample.h>

using namespace villas::utils;

static int io_print_lines(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;
	unsigned i;

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
	int ret;
	unsigned i;

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

int io_init(struct io *io, const struct format_type *fmt, struct vlist *signals, int flags)
{
	int ret;

	assert(io->state == State::DESTROYED);

	io->_vt = fmt;
	io->_vd = alloc(fmt->size);

	io->flags = flags | (io_type(io)->flags & ~(int) SampleFlags::HAS_ALL);
	io->delimiter = io_type(io)->delimiter ? io_type(io)->delimiter : '\n';
	io->separator = io_type(io)->separator ? io_type(io)->separator : '\t';

	io->in.buflen =
	io->out.buflen = 4096;

	io->in.buffer = (char *) alloc(io->in.buflen);
	io->out.buffer = (char *) alloc(io->out.buflen);

	io->signals = signals;

	ret = io_type(io)->init ? io_type(io)->init(io) : 0;
	if (ret)
		return ret;

	io->state = State::INITIALIZED;

	return 0;
}

int io_init2(struct io *io, const struct format_type *fmt, const char *dt, int flags)
{
	int ret;
	struct vlist *signals;

	signals = (struct vlist *) alloc(sizeof(struct vlist));
	signals->state = State::DESTROYED;

	ret = vlist_init(signals);
	if (ret)
		return ret;

	ret = signal_list_generate2(signals, dt);
	if (ret)
		return ret;

	flags |= (int) IOFlags::DESTROY_SIGNALS;

	return io_init(io, fmt, signals, flags);
}

int io_destroy(struct io *io)
{
	int ret;

	assert(io->state == State::CLOSED || io->state == State::INITIALIZED || io->state == State::CHECKED);

	ret = io_type(io)->destroy ? io_type(io)->destroy(io) : 0;
	if (ret)
		return ret;

	free(io->_vd);
	free(io->in.buffer);
	free(io->out.buffer);

	if (io->flags & (int) IOFlags::DESTROY_SIGNALS) {
		ret = vlist_destroy(io->signals, (dtor_cb_t) signal_decref, false);
		if (ret)
			return ret;
	}

	io->state = State::DESTROYED;

	return 0;
}

int io_check(struct io *io)
{
	assert(io->state != State::DESTROYED);

	io->state = State::CHECKED;

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
			io->mode = IOMode::STDIO;

			io->out.stream.std = fopen(uri, "a+");
			if (io->out.stream.std == nullptr)
				return -1;

			io->in.stream.std  = fopen(uri, "r");
			if (io->in.stream.std == nullptr)
				return -1;
		}
		else {
			io->mode = IOMode::ADVIO;

			io->out.stream.adv = afopen(uri, "a+");
			if (io->out.stream.adv == nullptr)
				return -1;

			io->in.stream.adv = afopen(uri, "a+");
			if (io->in.stream.adv == nullptr)
				return -2;
		}
	}
	else {
stdio:		io->mode = IOMode::STDIO;
		io->flags |= (int) IOFlags::FLUSH;

		io->in.stream.std  = stdin;
		io->out.stream.std = stdout;
	}

	/* Make stream non-blocking if desired */
	if (io->flags & (int) IOFlags::NONBLOCK) {
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
	if (io->mode == IOMode::STDIO) {
		ret = setvbuf(io->in.stream.std, nullptr, _IOLBF, BUFSIZ);
		if (ret)
			return -1;

		ret = setvbuf(io->out.stream.std, nullptr, _IOLBF, BUFSIZ);
		if (ret)
			return -1;
	}

	return 0;
}

int io_stream_close(struct io *io)
{
	int ret;

	switch (io->mode) {
		case IOMode::ADVIO:
			ret = afclose(io->in.stream.adv);
			if (ret)
				return ret;

			ret = afclose(io->out.stream.adv);
			if (ret)
				return ret;

			return 0;

		case IOMode::STDIO:
			if (io->in.stream.std == stdin)
				return 0;

			ret = fclose(io->in.stream.std);
			if (ret)
				return ret;

			ret = fclose(io->out.stream.std);
			if (ret)
				return ret;

			return 0;

		case IOMode::CUSTOM:
			return 0;
	}

	return -1;
}

int io_stream_flush(struct io *io)
{
	switch (io->mode) {
		case IOMode::ADVIO:
			return afflush(io->out.stream.adv);
		case IOMode::STDIO:
			return fflush(io->out.stream.std);
		case IOMode::CUSTOM:
			return 0;
	}

	return -1;
}

int io_stream_eof(struct io *io)
{
	switch (io->mode) {
		case IOMode::ADVIO:
			return afeof(io->in.stream.adv);
		case IOMode::STDIO:
			return feof(io->in.stream.std);
		case IOMode::CUSTOM:
			return 0;
	}

	return -1;
}

void io_stream_rewind(struct io *io)
{
	switch (io->mode) {
		case IOMode::ADVIO:
			arewind(io->in.stream.adv);
			break;
		case IOMode::STDIO:
			rewind(io->in.stream.std);
			break;
		case IOMode::CUSTOM: { }
	}
}

int io_stream_fd(struct io *io)
{
	switch (io->mode) {
		case IOMode::ADVIO:
			return afileno(io->in.stream.adv);
		case IOMode::STDIO:
			return fileno(io->in.stream.std);
		case IOMode::CUSTOM:
			return -1;
	}

	return -1;
}

int io_open(struct io *io, const char *uri)
{
	int ret;

	assert(io->state == State::CHECKED || io->state == State::CLOSED);

	ret = io_type(io)->open
		? io_type(io)->open(io, uri)
		: io_stream_open(io, uri);
	if (ret)
		return ret;

	io->header_printed = false;
	io->state = State::OPENED;

	return 0;
}

int io_close(struct io *io)
{
	int ret;

	assert(io->state == State::OPENED);

	io_footer(io);

	ret = io_type(io)->close
		? io_type(io)->close(io)
	 	: io_stream_close(io);
	if (ret)
		return ret;

	io->state = State::CLOSED;

	return 0;
}

int io_flush(struct io *io)
{
	assert(io->state == State::OPENED);

	return io_type(io)->flush
		? io_type(io)->flush(io)
		: io_stream_flush(io);
}

int io_eof(struct io *io)
{
	assert(io->state == State::OPENED);

	return io_type(io)->eof
		? io_type(io)->eof(io)
		: io_stream_eof(io);
}

void io_rewind(struct io *io)
{
	assert(io->state == State::OPENED);

	if (io_type(io)->rewind)
		io_type(io)->rewind(io);
	else
		io_stream_rewind(io);
}

int io_fd(struct io *io)
{
	assert(io->state == State::OPENED);

	return io_type(io)->fd
		? io_type(io)->fd(io)
		: io_stream_fd(io);
}

const struct format_type * io_type(struct io *io)
{
	return io->_vt;
}

void io_header(struct io *io, const struct sample *smp)
{
	assert(io->state == State::OPENED);

	if (io_type(io)->header)
		io_type(io)->header(io, smp);

	io->header_printed = true;
}

void io_footer(struct io *io)
{
	assert(io->state == State::OPENED);

	if (io_type(io)->footer)
		io_type(io)->footer(io);
}

int io_print(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;

	assert(io->state == State::OPENED);

	if (!io->header_printed && cnt > 0)
		io_header(io, smps[0]);

	if (io->flags & (int) IOFlags::NEWLINES)
		ret = io_print_lines(io, smps, cnt);
	else if (io_type(io)->print)
		ret = io_type(io)->print(io, smps, cnt);
	else if (io_type(io)->sprint) {
		FILE *f = io_stream_output(io);
		size_t wbytes;

		ret = io_sprint(io, io->out.buffer, io->out.buflen, &wbytes, smps, cnt);

		fwrite(io->out.buffer, wbytes, 1, f);
	}
	else
		ret = -1;

	if (io->flags & (int) IOFlags::FLUSH)
		io_flush(io);

	return ret;
}

int io_scan(struct io *io, struct sample *smps[], unsigned cnt)
{
	int ret;

	assert(io->state == State::OPENED);

	if (io->flags & (int) IOFlags::NEWLINES)
		ret = io_scan_lines(io, smps, cnt);
	else if (io_type(io)->scan)
		ret = io_type(io)->scan(io, smps, cnt);
	else if (io_type(io)->sscan) {
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
	if (io->state != State::OPENED)
		return 0;

	return io->mode == IOMode::ADVIO
		? io->out.stream.adv->file
		: io->out.stream.std;
}

FILE * io_stream_input(struct io *io) {
	if (io->state != State::OPENED)
		return 0;

	return io->mode == IOMode::ADVIO
		? io->in.stream.adv->file
		: io->in.stream.std;
}

int io_sscan(struct io *io, const char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt)
{
	assert(io->state == State::CHECKED || io->state == State::OPENED);

	return io_type(io)->sscan ? io_type(io)->sscan(io, buf, len, rbytes, smps, cnt) : -1;
}

int io_sprint(struct io *io, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt)
{
	assert(io->state == State::CHECKED || io->state == State::OPENED);

	return io_type(io)->sprint ? io_type(io)->sprint(io, buf, len, wbytes, smps, cnt) : -1;
}
