/** Print fancy tables
 *
 * @file
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

/** @addtogroup table Print fancy tables
 * @{
 */

#pragma once

#include <vector>
#include <string>

class Table;

class TableColumn {

	friend Table;

public:
	enum class Alignment {
		LEFT,
		RIGHT
	};

protected:
	int _width;	/**< The real width of this column. Calculated by Table::resize() */

	int width;	/**< Width of the column. */

public:
	TableColumn(int w, enum Alignment a, const std::string &t, const std::string &f, const std::string &u = "") :
		width(w),
		title(t),
		format(f),
		unit(u),
		align(a)
	{ }

	std::string title;	/**< The title as shown in the table header. */
	std::string format;	/**< The format which is used to print the table rows. */
	std::string unit;	/**< An optional unit which will be shown in the table header. */

	enum Alignment align;

	int getWidth() const
	{
		return _width;
	}
};

class Table {

protected:
	int resize(int w);

	int width;

	std::vector<TableColumn> columns;

public:
	Table(const std::vector<TableColumn> &cols) :
		width(-1),
		columns(cols)
	{ }

	/** Print a table header consisting of \p n columns. */
	void header();

	/** Print table rows. */
	void row(int count, ...);

	int getWidth() const
	{
		return width;
	}
};

/** @} */
