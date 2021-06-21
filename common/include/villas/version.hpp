/** Version.
 *
 * @file
 * @author Steffen Vogel <github@daniel-krebs.net>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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

#pragma once

#include <string>

namespace villas {
namespace utils {

class Version {

protected:
	int components[3];

	static int cmp(const Version &lhs, const Version &rhs);

public:
	/** Parse a dotted version string. */
	Version(const std::string &s);

	Version(int maj, int min = 0, int pat = 0);

	inline bool operator==(const Version &rhs) { return cmp(*this, rhs) == 0; }
	inline bool operator!=(const Version &rhs) { return cmp(*this, rhs) != 0; }
	inline bool operator< (const Version &rhs) { return cmp(*this, rhs) <  0; }
	inline bool operator> (const Version &rhs) { return cmp(*this, rhs) >  0; }
	inline bool operator<=(const Version &rhs) { return cmp(*this, rhs) <= 0; }
	inline bool operator>=(const Version &rhs) { return cmp(*this, rhs) >= 0; }
};

} /* namespace villas */
} /* namespace utils */
