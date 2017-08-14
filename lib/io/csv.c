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
#include <inttypes.h>

#include "io/csv.h"
#include "plugin.h"
#include "sample.h"
#include "timing.h"

int csv_fprint_single(FILE *f, struct sample *s, int flags)
{
	fprintf(f, "%ld %09ld %d", s->ts.origin.tv_sec, s->ts.origin.tv_nsec, s->sequence);

	for (int i = 0; i < s->length; i++) {
		switch ((s->format >> i) & 0x1) {
			case SAMPLE_DATA_FORMAT_FLOAT:
				fprintf(f, "%c%.6f", CSV_SEPARATOR, s->data[i].f);
				break;
			case SAMPLE_DATA_FORMAT_INT:
				fprintf(f, "%c%" PRId64, CSV_SEPARATOR, s->data[i].i);
				break;
		}
	}

	fputc('\n', f);

	return 0;
}

size_t csv_sscan_single(const char *buf, size_t len, struct sample *s, int *flags)
{
	int ret, off;

	ret = sscanf(buf, "%ld %ld %d %n", &s->ts.origin.tv_sec, &s->ts.origin.tv_nsec, &s->sequence, &off);
	if (ret != 3)
		return -1;

	int i;
	for (i = 0; i < s->capacity; i++) {
		switch (s->format & (1 << i)) {
			case SAMPLE_DATA_FORMAT_FLOAT:
				ret = sscanf(buf + off, "%lf %n", &s->data[i].f, &off);
				break;
			case SAMPLE_DATA_FORMAT_INT:
				ret = sscanf(buf + off, "%" PRId64 " %n", &s->data[i].i, &off);
				break;
		}

		if (ret != 2)
			break;
	}

	s->length = i;
	s->ts.received = time_now();

	return 0;
}

int csv_fscan_single(FILE *f, struct sample *s, int *flags)
{
	char *ptr, line[4096];

skip:	if (fgets(line, sizeof(line), f) == NULL)
		return -1; /* An error occured */

	/* Skip whitespaces, empty and comment lines */
	for (ptr = line; isspace(*ptr); ptr++);
	if (*ptr == '\0' || *ptr == '#')
		goto skip;

	return csv_sscan_single(line, strlen(line), s, flags);
}

int csv_fprint(FILE *f, struct sample *smps[], unsigned cnt, int flags)
{
	int ret, i;
	for (i = 0; i < cnt; i++) {
		ret = csv_fprint_single(f, smps[i], flags);
		if (ret < 0)
			break;
	}

	return i;
}

int csv_fscan(FILE *f, struct sample *smps[], unsigned cnt, int *flags)
{
	int ret, i;
	for (i = 0; i < cnt; i++) {
		ret = csv_fscan_single(f, smps[i], flags);
		if (ret < 0) {
			warn("Failed to read CSV line: %d", ret);
			break;
		}
	}

	return i;
}

static struct plugin p = {
	.name = "csv",
	.description = "Tabulator-separated values",
	.type = PLUGIN_TYPE_IO,
	.io = {
		.fprint	= csv_fprint,
		.fscan	= csv_fscan,
		.size = 0
	}
};

REGISTER_PLUGIN(&p);
