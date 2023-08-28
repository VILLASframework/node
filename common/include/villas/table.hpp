/* Print fancy tables
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 */

#pragma once

#include <vector>
#include <string>

#include <villas/log.hpp>

namespace villas {

class Table;

class TableColumn {

	friend Table;

public:
	enum class Alignment {
		LEFT,
		RIGHT
	};

protected:
	int _width;	// The real width of this column. Calculated by Table::resize().

	int width;	// Width of the column.

public:
	TableColumn(int w, enum Alignment a, const std::string &t, const std::string &f, const std::string &u = "") :
		_width(0),
		width(w),
		title(t),
		format(f),
		unit(u),
		align(a)
	{ }

	std::string title;	// The title as shown in the table header.
	std::string format;	// The format which is used to print the table rows.
	std::string unit;	// An optional unit which will be shown in the table header.

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

	Logger logger;

public:
	Table(Logger log, const std::vector<TableColumn> &cols) :
		width(-1),
		columns(cols),
		logger(log)
	{ }

	// Print a table header consisting of \p n columns.
	void header();

	// Print table rows.
	void row(int count, ...);

	int getWidth() const
	{
		return width;
	}
};

} // namespace villas
