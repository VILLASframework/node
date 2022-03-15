/** Signal type.
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

#include <string>

namespace villas {
namespace node {

enum class SignalType {
	INVALID	= 0,	/**< Signal type is invalid. */
	FLOAT	= 1,	/**< See SignalData::f */
	INTEGER	= 2,	/**< See SignalData::i */
	BOOLEAN = 3,	/**< See SignalData::b */
	COMPLEX = 4	/**< See SignalData::z */
};

enum SignalType signalTypeFromString(const std::string &str);

enum SignalType signalTypeFromFormatString(char c);

std::string signalTypeToString(enum SignalType fmt);

enum SignalType signalTypeDetect(const std::string &val);

} /* namespace node */
} /* namespace villas */
