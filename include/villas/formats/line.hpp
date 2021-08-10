/** Line-based formats
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/format.hpp>

namespace villas {
namespace node {

class LineFormat : public Format {

protected:
	virtual size_t sprintLine(char *buf, size_t len, const struct Sample *smp) = 0;
	virtual size_t sscanLine(const char *buf, size_t len, struct Sample *smp) = 0;

	char delimiter;		/**< Newline delimiter. */
	char comment;		/**< Prefix for comment lines. */

	bool skip_first_line;	/**< While reading, the first line is skipped (header) */
	bool print_header;	/**< Before any data, a header line is printed */

	bool first_line_skipped;
	bool header_printed;

public:
	LineFormat(int fl, char delim = '\n', char com = '#') :
		Format(fl),
		delimiter(delim),
		comment(com),
		skip_first_line(false),
		print_header(true),
		first_line_skipped(false),
		header_printed(false)
	{ }

	/** Print a header. */
	virtual
	void header(FILE *f, const SignalList::Ptr sigs)
	{
		header_printed = true;
	}

	virtual
	int sprint(char *buf, size_t len, size_t *wbytes, const struct Sample * const smps[], unsigned cnt);
	virtual
	int sscan(const char *buf, size_t len, size_t *rbytes, struct Sample * const smps[], unsigned cnt);

	virtual
	int scan(FILE *f, struct Sample * const smps[], unsigned cnt);
	virtual
	int print(FILE *f, const struct Sample * const smps[], unsigned cnt);

	virtual
	void parse(json_t *json);
};

template <typename T, const char *name, const char *desc, int flags = 0, char delimiter = '\n'>
class LineFormatPlugin : public FormatFactory {

public:
	using FormatFactory::FormatFactory;

	virtual
	Format * make()
	{
		return new T(flags, delimiter);
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
