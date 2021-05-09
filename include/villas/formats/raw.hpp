/** RAW IO format
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

#include <sstream>
#include <cstdlib>

#include <villas/format.hpp>

/* float128 is currently not yet supported as htole128() functions a missing */
#if 0 && defined(__GNUC__) && defined(__linux__)
  #define HAS_128BIT
#endif

/* Forward declarations */
struct sample;

namespace villas {
namespace node {

class RawFormat : public BinaryFormat {

public:
	enum Endianess {
		BIG,
		LITTLE
	};

protected:

	enum Endianess endianess;
	int bits;
	bool fake;

public:
	RawFormat(int fl, int b = 32, enum Endianess e = Endianess::LITTLE) :
		BinaryFormat(fl),
		endianess(e),
		bits(b),
		fake(false)
	{
		if (fake)
			flags |= (int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_TS_ORIGIN;
	}

	int sscan(const char *buf, size_t len, size_t *rbytes, struct sample * const smps[], unsigned cnt);
	int sprint(char *buf, size_t len, size_t *wbytes, const struct sample * const smps[], unsigned cnt);

	virtual void parse(json_t *json);
};

class GtnetRawFormat : public RawFormat {

public:
	GtnetRawFormat(int fl) :
		RawFormat(fl, 32, Endianess::BIG)
	{ }
};

} /* namespace node */
} /* namespace villas */
