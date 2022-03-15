/** Comma-separated values.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <cstdlib>

#include <villas/formats/line.hpp>

namespace villas {
namespace node {

/* Forward declarations. */
struct Sample;

class ColumnLineFormat : public LineFormat {

protected:
	virtual size_t sprintLine(char *buf, size_t len, const struct Sample *smp);
	virtual size_t sscanLine(const char *buf, size_t len, struct Sample *smp);

	char separator;		/**< Column separator */

public:
	ColumnLineFormat(int fl, char delim, char sep) :
		LineFormat(fl, delim),
		separator(sep)
	{ }

	virtual
	void header(FILE *f, const SignalList::Ptr sigs);

	virtual
	void parse(json_t *json);
};

template <const char *name, const char *desc, int flags = 0, char delimiter = '\n', char separator = '\t'>
class ColumnLineFormatPlugin : public FormatFactory {

public:
	using FormatFactory::FormatFactory;

	virtual
	Format * make()
	{
		return new ColumnLineFormat(flags, delimiter, separator);
	}

	/// Get plugin name
	virtual
	std::string getName() const
	{
		return name;
	}

	/// Get plugin description
	virtual
	std::string getDescription() const
	{
		return desc;
	}
};

} /* namespace node */
} /* namespace villas */
