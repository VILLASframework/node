/** Version.
 *
 * @file
 * @author Steffen Vogel <github@daniel-krebs.net>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache License 2.0
 *********************************************************************************/

#pragma once

#include <string>

namespace villas {
namespace utils {

class Version {

protected:
	int components[3];

	static
	int cmp(const Version &lhs, const Version &rhs);

public:
	// Parse a dotted version string.
	Version(const std::string &s);

	Version(int maj, int min = 0, int pat = 0);

	inline
	bool operator==(const Version &rhs)
	{
		return cmp(*this, rhs) == 0;
	}

	inline
	bool operator!=(const Version &rhs)
	{
		return cmp(*this, rhs) != 0;
	}

	inline
	bool operator< (const Version &rhs)
	{
		return cmp(*this, rhs) <  0;
	}

	inline
	bool operator> (const Version &rhs)
	{
		return cmp(*this, rhs) >  0;
	}

	inline
	bool operator<=(const Version &rhs)
	{
		return cmp(*this, rhs) <= 0;
	}

	inline
	bool operator>=(const Version &rhs)
	{
		return cmp(*this, rhs) >= 0;
	}
};

} // namespace villas
} // namespace utils
