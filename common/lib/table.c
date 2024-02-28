/** Print fancy tables.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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
#include <string.h>

#include <villas/utils.h>
#include <villas/table.h>
#include <villas/log.h>

static int table_resize(struct table *t, int width)
{
	int norm, flex, fixed, total;

	t->width = width;

	norm  = 0;
	flex  = 0;
	fixed = 0;
	total = t->width - t->ncols * 2;

	/* Normalize width */
	for (int i = 0; i < t->ncols; i++) {
		if (t->cols[i].width > 0)
			norm += t->cols[i].width;
		if (t->cols[i].width == 0)
			flex++;
		if (t->cols[i].width < 0)
			fixed += -1 * t->cols[i].width;
	}

	for (int i = 0; i < t->ncols; i++) {
		if (t->cols[i].width > 0)
			t->cols[i]._width = t->cols[i].width * (float) (total - fixed) / norm;
		if (t->cols[i].width == 0)
			t->cols[i]._width = (float) (total - fixed) / flex;
		if (t->cols[i].width < 0)
			t->cols[i]._width = -1 * t->cols[i].width;
	}

	return 0;
}

void table_header(struct table *t)
{
	if (t->width != log_get_width())
		table_resize(t, log_get_width());

	char *line1 = strf("\b\b" BOX_UD);
	char *line2 = strf("\b\b" BOX_UD);
	char *line3 = strf("\b");

	for (int i = 0; i < t->ncols; i++) {
		int w, u;
		char *col, *unit;

		col = strf(CLR_BLD("%s"), t->cols[i].title);
		unit = t->cols[i].unit ? strf(CLR_YEL("%s"), t->cols[i].unit) : "";

		w = t->cols[i]._width + strlen(col) - strlenp(col);
		u = t->cols[i]._width + strlen(unit) - strlenp(unit);

		if (t->cols[i].align == TABLE_ALIGN_LEFT) {
			strcatf(&line1, " %-*.*s\e[0m " BOX_UD, w, w, col);
			strcatf(&line2, " %-*.*s\e[0m " BOX_UD, u, u, unit);
		}
		else {
			strcatf(&line1, " %*.*s\e[0m " BOX_UD, w, w, col);
			strcatf(&line2, " %*.*s\e[0m " BOX_UD, u, u, unit);
		}

		for (int j = 0; j < t->cols[i]._width + 2; j++) {
			strcatf(&line3, "%s", BOX_LR);
		}

		if (i == t->ncols - 1) {
			strcatf(&line3, "%s", BOX_UDL);
		}
		else {
			strcatf(&line3, "%s", BOX_UDLR);
		}

		free(col);
	}

	info("%s", line1);
	info("%s", line2);
	info("%s", line3);

	free(line1);
	free(line2);
	free(line3);
}

void table_row(struct table *t, ...)
{
	if (t->width != log_get_width()) {
		table_resize(t, log_get_width());
		table_header(t);
	}

	va_list args;
	va_start(args, t);

	char *line = strf("\b\b" BOX_UD);

	for (int i = 0; i < t->ncols; ++i) {
		char *col = vstrf(t->cols[i].format, args);

		int l = strlenp(col);
		int r = strlen(col);
		int w = t->cols[i]._width + r - l;

		if (t->cols[i].align == TABLE_ALIGN_LEFT)
			strcatf(&line, " %-*.*s\e[0m " BOX_UD, w, w, col);
		else
			strcatf(&line, " %*.*s\e[0m " BOX_UD, w, w, col);

		free(col);
	}

	va_end(args);

	info("%s", line);
	free(line);
}
