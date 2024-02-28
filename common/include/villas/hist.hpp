/** Histogram class.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <vector>

#include <jansson.h>

#define HEIGHT	(LOG_WIDTH - 55)
#define SEQ	17

namespace villas {

/** Histogram structure used to collect statistics. */
class Hist {

public:
	using cnt_t = uintmax_t;
	using idx_t = std::vector<cnt_t>::difference_type;

	/** Initialize struct hist with supplied values and allocate memory for buckets. */
	Hist(int buckets = 0, cnt_t warmup = 0);

	/** Reset all counters and values back to zero. */
	void reset();

	/** Count a value within its corresponding bucket. */
	void put(double value);

	/** Calcluate the variance of all counted values. */
	double getVar() const;

	/** Calculate the mean average of all counted values. */
	double getMean() const;

	/** Calculate the standard derivation of all counted values. */
	double getStddev() const;

	/** Print all statistical properties of distribution including a graphilcal plot of the histogram. */
	void print(bool details) const;

	/** Print ASCII style plot of histogram */
	void plot() const;

	/** Dump histogram data in Matlab format.
	 *
	 * @return The string containing the dump. The caller is responsible to free() the buffer.
	 */
	char * dump() const;

	/** Prints Matlab struct containing all infos to file. */
	int dumpMatlab(FILE *f) const;

	/** Write the histogram in JSON format to fiel \p f. */
	int dumpJson(FILE *f) const;

	/** Build a libjansson / JSON object of the histogram. */
	json_t * toJson() const;

	double getHigh() const
	{
		return high;
	}

	double getLow() const
	{
		return low;
	}

	double getHighest() const
	{
		return highest;
	}

	double getLowest() const
	{
		return lowest;
	}

	double getLast() const
	{
		return last;
	}

	cnt_t getTotal() const
	{
		return total;
	}

protected:
	double resolution;	/**< The distance between two adjacent buckets. */

	double high;		/**< The value of the highest bucket. */
	double low;		/**< The value of the lowest bucket. */

	double highest;		/**< The highest value observed (may be higher than #high). */
	double lowest;		/**< The lowest value observed (may be lower than #low). */
	double last;		/**< The last value which has been put into the buckets */

	cnt_t total;		/**< Total number of counted values. */
	cnt_t warmup;		/**< Number of values which are used during warmup phase. */

	cnt_t higher;		/**< The number of values which are higher than #high. */
	cnt_t lower;		/**< The number of values which are lower than #low. */


	std::vector<cnt_t> data; /**< Bucket counters. */

	double _m[2], _s[2];	/**< Private variables for online variance calculation */
};

} /* namespace villas */
