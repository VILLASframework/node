/** Signal types.
 *
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

#include <cstring>

#include <villas/signal_type.hpp>

using namespace villas::node;

enum SignalType villas::node::signalTypeFromString(const std::string &str)
{
	if      (str == "boolean")
		return SignalType::BOOLEAN;
	else if (str == "complex")
		return SignalType::COMPLEX;
	else if (str == "float")
		return SignalType::FLOAT;
	else if (str == "integer")
		return SignalType::INTEGER;
	else
		return SignalType::INVALID;
}

enum SignalType villas::node::signalTypeFromFormatString(char c)
{
	switch (c) {
		case 'f':
			return SignalType::FLOAT;

		case 'i':
			return SignalType::INTEGER;

		case 'c':
			return SignalType::COMPLEX;

		case 'b':
			return SignalType::BOOLEAN;

		default:
			return SignalType::INVALID;
	}
}

std::string villas::node::signalTypeToString(enum SignalType fmt)
{
	switch (fmt) {
		case SignalType::BOOLEAN:
			return "boolean";

		case SignalType::COMPLEX:
			return "complex";

		case SignalType::FLOAT:
			return "float";

		case SignalType::INTEGER:
			return "integer";

		case SignalType::INVALID:
			return "invalid";
	}

	return nullptr;
}

enum SignalType villas::node::signalTypeDetect(const std::string &val)
{
	int len;

	if (val.find('i') != std::string::npos)
		return SignalType::COMPLEX;

	if (val.find(',') != std::string::npos)
		return SignalType::FLOAT;

	len = val.size();
	if (len == 1 && (val[0] == '1' || val[0] == '0'))
		return SignalType::BOOLEAN;

	return SignalType::INTEGER;
}
