/** Signal data.
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

#include <jansson.h>

#include <limits>
#include <complex>

#include <cstdint>

/** A signal value.
 *
 * Data is in host endianess!
 */
union signal_data {
	double f;		/**< Floating point values. */
	int64_t i;		/**< Integer values. */
	bool b;			/**< Boolean values. */
	std::complex<float> z;	/**< Complex values. */

	signal_data() :
		i(0)
	{ }

	static union signal_data nan()
	{
		union signal_data d;

		d.f = std::numeric_limits<double>::quiet_NaN();

		return d;
	}

	bool is_nan()
	{
		return f == std::numeric_limits<double>::quiet_NaN();
	}
};

/** Convert signal data from one description/format to another. */
void signal_data_cast(union signal_data *data, enum SignalType from, enum SignalType to);

/** Print value of a signal to a character buffer. */
int signal_data_print_str(const union signal_data *data, enum SignalType type, char *buf, size_t len, int precision = 5);

int signal_data_parse_str(union signal_data *data, enum SignalType type, const char *ptr, char **end);

int signal_data_parse_json(union signal_data *data, enum SignalType type, json_t *json);

json_t * signal_data_to_json(const union signal_data *data, enum SignalType type);

void signal_data_set(union signal_data *data, enum SignalType type, double val);
