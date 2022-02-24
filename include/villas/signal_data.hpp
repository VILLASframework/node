/** Signal data.
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

#include <jansson.h>

#include <limits>
#include <complex>
#include <string>

#include <cstdint>

namespace villas {
namespace node {

/** A signal value.
 *
 * Data is in host endianess!
 */
union SignalData {
	double f;		/**< Floating point values. */
	int64_t i;		/**< Integer values. */
	bool b;			/**< Boolean values. */
	std::complex<float> z;	/**< Complex values. */

	SignalData() :
		i(0)
	{ }

	static union SignalData nan()
	{
		union SignalData d;

		d.f = std::numeric_limits<double>::quiet_NaN();

		return d;
	}

	bool is_nan()
	{
		return f == std::numeric_limits<double>::quiet_NaN();
	}

	/** Convert signal data from one description/format to another. */
	SignalData cast(enum SignalType type, enum SignalType to) const;

	/** Set data from double */
	void set(enum SignalType type, double val);

	/** Print value of a signal to a character buffer. */
	int printString(enum SignalType type, char *buf, size_t len, int precision = 5) const;

	int parseString(enum SignalType type, const char *ptr, char **end);

	int parseJson(enum SignalType type, json_t *json);

	json_t * toJson(enum SignalType type) const;

	std::string toString(enum SignalType type, int precision = 5) const;
};

} /* namespace node */
} /* namespace villas */
