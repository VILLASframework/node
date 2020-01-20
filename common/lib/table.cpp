/** Print fancy tables.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <cstdlib>
#include <cstring>

#include <villas/utils.hpp>
#include <villas/table.hpp>
#include <villas/colors.hpp>
#include <villas/boxes.hpp>
#include <villas/log.h>

using namespace villas::utils;

int Table::resize(int w)
{
	int norm, flex, fixed, total;

	width = w;

	norm  = 0;
	flex  = 0;
	fixed = 0;
	total = width - columns.size() * 2;

	/* Normalize width */
	for (unsigned i = 0; i < columns.size(); i++) {
		if (columns[i].width > 0)
			norm += columns[i].width;
		if (columns[i].width == 0)
			flex++;
		if (columns[i].width < 0)
			fixed += -1 * columns[i].width;
	}

	for (unsigned i = 0; i < columns.size(); i++) {
		if (columns[i].width > 0)
			columns[i]._width = columns[i].width * (float) (total - fixed) / norm;
		if (columns[i].width == 0)
			columns[i]._width = (float) (total - fixed) / flex;
		if (columns[i].width < 0)
			columns[i]._width = -1 * columns[i].width;
	}

	return 0;
}

void Table::header()
{
	if (width != log_get_width())
		resize(log_get_width());

	char *line1 = nullptr;
	char *line2 = nullptr;
	char *line3 = nullptr;

	for (unsigned i = 0; i < columns.size(); i++) {
		int w, u;
		char *col, *unit;

		col = strf(CLR_BLD("%s"), columns[i].title.c_str());
		unit = columns[i].unit.size() ? strf(CLR_YEL("%s"), columns[i].unit.c_str()) : strf("");

		w = columns[i]._width + strlen(col) - strlenp(col);
		u = columns[i]._width + strlen(unit) - strlenp(unit);

		if (columns[i].align == TableColumn::Alignment::LEFT) {
			strcatf(&line1, " %-*.*s\e[0m", w, w, col);
			strcatf(&line2, " %-*.*s\e[0m", u, u, unit);
		}
		else {
			strcatf(&line1, " %*.*s\e[0m", w, w, col);
			strcatf(&line2, " %*.*s\e[0m", u, u, unit);
		}

		for (int j = 0; j < columns[i]._width + 2; j++) {
			strcatf(&line3, "%s", BOX_LR);
		}

		if (i != columns.size() - 1) {
			strcatf(&line1, " %s", BOX_UD);
			strcatf(&line2, " %s", BOX_UD);
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

void Table::row(int count, ...)
{
	if (width != log_get_width()) {
		resize(log_get_width());
		header();
	}

	va_list args;
	va_start(args, count);

	char *line = nullptr;

	for (unsigned i = 0; i < columns.size(); ++i) {
		char *col = vstrf(columns[i].format.c_str(), args);

		int l = strlenp(col);
		int r = strlen(col);
		int w = columns[i]._width + r - l;

		if (columns[i].align == TableColumn::Alignment::LEFT)
			strcatf(&line, " %-*.*s\e[0m ", w, w, col);
		else
			strcatf(&line, " %*.*s\e[0m ", w, w, col);

		if (i != columns.size() - 1)
			strcatf(&line, BOX_UD);

		free(col);
	}

	va_end(args);

	info("%s", line);
	free(line);
}
