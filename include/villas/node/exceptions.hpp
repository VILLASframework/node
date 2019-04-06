/** VILLASnode exceptions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/exceptions.hpp>

namespace villas {
namespace node {

class ParseError : public RuntimeError {

protected:
	std::string text;
	std::string file;
	int line;
	int column;

public:
	ParseError(const std::string &t, const std::string &f, int l, int c = 0) :
		RuntimeError("Failed to parse configuration: {} in {}:{}", t, f, l),
		text(t),
		file(f),
		line(l),
		column(c)
	{ }
};

#ifdef WITH_CONFIG
class LibconfigParseError : public ParseError {

protected:
	const config_t *config;

public:
	LibconfigParseError(const config_t *c) :
		ParseError(
			config_error_text(c),
			config_error_file(c) ? config_error_file(c) : "<unknown>",
			config_error_line(c)
		),
		config(c)
	{ }
};
#endif /* WITH_CONFIG */

class JanssonParseError : public ParseError {

protected:
	json_error_t error;

public:
	JanssonParseError(json_error_t e) :
		ParseError(
			e.text,
			e.source,
			e.line,
			e.column
		),
		error(e)
	{ }
};

} // namespace node
} // namespace villas
