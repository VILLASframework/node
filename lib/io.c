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

#include "io.h"
#include "utils.h"

int io_init(struct io *io, struct io_format *fmt, int flags)
{
	io->_vt = fmt;
	io->_vd = alloc(fmt->size);

	io->flags = flags;

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

int io_open(struct io *io, const char *uri, const char *mode)
{
	if (io->_vt->open)
		return io->_vt->open(io, uri, mode);
	else {
		io->file = afopen(uri, mode);
		if (!io->file)
			return -1;

		return 0;
	}
}

int io_close(struct io *io)
{
	return io->_vt->close ? io->_vt->close(io) : afclose(io->file);
}

int io_print(struct io *io, struct sample *smps[], size_t cnt)
{
	assert(io->_vt->print);

	for (int i = 0; i < cnt; i++)
		io->_vt->print(io, smps[i], io->flags);

	return cnt;
}

int io_scan(struct io *io, struct sample *smps[], size_t cnt)
{
	int ret;

	assert(io->_vt->scan);

	for (int i = 0; i < cnt && !io_eof(io); i++) {
		ret = io->_vt->scan(io, smps[i], NULL);
		if (ret < 0)
			return i;
	}

	return cnt;
}

int io_eof(struct io *io)
{
	return io->_vt->eof ? io->_vt->eof(io) : afeof(io->file);
}

void io_rewind(struct io *io)
{
	io->_vt->rewind ? io->_vt->rewind(io) : arewind(io->file);
}
