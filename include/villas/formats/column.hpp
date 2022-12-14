/** Comma-separated values.
 *
 * @file
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
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
