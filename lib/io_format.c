/** Read / write sample data in different formats.
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

#include "plugin.h"
#include "io_format.h"

int io_format_sscan(struct io_format *fmt, char *buf, size_t len, size_t *rbytes, struct sample *smps[], unsigned cnt, int flags)
{
	flags |= fmt->flags;

	return fmt->sscan ? fmt->sscan(buf, len, rbytes, smps, cnt, flags) : -1;
}

int io_format_sprint(struct io_format *fmt, char *buf, size_t len, size_t *wbytes, struct sample *smps[], unsigned cnt, int flags)
{
	flags |= fmt->flags;

	return fmt->sprint ? fmt->sprint(buf, len, wbytes, smps, cnt, flags) : -1;
}

int io_format_fscan(struct io_format *fmt, FILE *f, struct sample *smps[], unsigned cnt, int flags)
{
	return fmt->sprint ? fmt->fscan(f, smps, cnt, flags) : -1;
}

int io_format_fprint(struct io_format *fmt, FILE *f, struct sample *smps[], unsigned cnt, int flags)
{
	return fmt->fprint ? fmt->fprint(f, smps, cnt, flags) : -1;
}

struct io_format * io_format_lookup(const char *name)
{
	struct plugin *p;

	p = plugin_lookup(PLUGIN_TYPE_IO, name);
	if (!p)
		return NULL;

	return &p->io;
}
