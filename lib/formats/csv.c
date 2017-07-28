/** Comma-separated values.
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

#include <ctype.h>

#include "formats/csv.h"
#include "plugin.h"
#include "sample.h"

int io_format_csv_fprint(AFILE *f, struct sample *s, int flags)
{
	afprintf(f, "%ld %09ld %d", s->ts.origin.tv_sec, s->ts.origin.tv_nsec, s->sequence);

	for (int i = 0; i < s->length; i++) {
		switch ((s->format >> i) & 0x1) {
			case SAMPLE_DATA_FORMAT_FLOAT:
				afprintf(f, "%c%.6f", CSV_SEPARATOR, s->data[i].f);
				break;
			case SAMPLE_DATA_FORMAT_INT:
				afprintf(f, "%c%d", CSV_SEPARATOR, s->data[i].i);
				break;
		}
	}

	return 0;
}

int io_format_csv_fscan(AFILE *f, struct sample *smp, int *flags)
{
	int ret, off;
	char *ptr, line[4096];

skip:	if (afgets(line, sizeof(line), f) == NULL)
		return -1; /* An error occured */

	/* Skip whitespaces, empty and comment lines */
	for (ptr = line; isspace(*ptr); ptr++);
	if (*ptr == '\0' || *ptr == '#')
		goto skip;

	ret = sscanf(line, "%ld %09ld %d %n", &smp->ts.origin.tv_sec, &smp->ts.origin.tv_nsec, &smp->sequence, &off);
	if (ret != 4)
		return -1;

	int i;
	for (i = 0; i < smp->capacity; i++) {
		switch (smp->format & (1 << i)) {
			case SAMPLE_DATA_FORMAT_FLOAT:
				ret = sscanf(line + off, "%f %n", &smp->data[i].f, &off);
				break;
			case SAMPLE_DATA_FORMAT_INT:
				ret = sscanf(line + off, "%d %n", &smp->data[i].i, &off);
				break;
		}

		if (ret != 2)
			break;
	}

	smp->length = i;

	return ret;
}

int io_format_csv_print(struct io *io, struct sample *smp, int flags)
{
	return io_format_csv_fprint(io->file, smp, flags);
}

int io_format_csv_scan(struct io *io, struct sample *smp, int *flags)
{
	return io_format_csv_fscan(io->file, smp, flags);
}

static struct plugin p = {
	.name = "csv",
	.description = "Tabulator-separated values",
	.type = PLUGIN_TYPE_FORMAT,
	.io = {
		.scan	= io_format_csv_scan,
		.print	= io_format_csv_print,
		.size = 0
	}
};

REGISTER_PLUGIN(&p);
