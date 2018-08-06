/** Histogram functions.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdio.h>
#include <stdint.h>

#include <jansson.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HIST_HEIGHT	(LOG_WIDTH - 55)
#define HIST_SEQ	17

typedef uintmax_t hist_cnt_t;

/** Histogram structure used to collect statistics. */
struct hist {
	double resolution;	/**< The distance between two adjacent buckets. */

	double high;		/**< The value of the highest bucket. */
	double low;		/**< The value of the lowest bucket. */

	double highest;		/**< The highest value observed (may be higher than #high). */
	double lowest;		/**< The lowest value observed (may be lower than #low). */
	double last;		/**< The last value which has been put into the buckets */

	int length;		/**< The number of buckets in #data. */

	hist_cnt_t total;	/**< Total number of counted values. */
	hist_cnt_t warmup;	/**< Number of values which are used during warmup phase. */

	hist_cnt_t higher;	/**< The number of values which are higher than #high. */
	hist_cnt_t lower;	/**< The number of values which are lower than #low. */

	hist_cnt_t *data;	/**< Pointer to dynamically allocated array of size length. */

	double _m[2], _s[2];	/**< Private variables for online variance calculation */
};

#define hist_last(h)	((h)->last)
#define hist_highest(h)	((h)->highest)
#define hist_lowest(h)	((h)->lowest)
#define hist_total(h)	((h)->total)

/** Initialize struct hist with supplied values and allocate memory for buckets. */
int hist_init(struct hist *h, int buckets, hist_cnt_t warmup);

/** Free the dynamically allocated memory. */
int hist_destroy(struct hist *h);

/** Reset all counters and values back to zero. */
void hist_reset(struct hist *h);

/** Count a value within its corresponding bucket. */
void hist_put(struct hist *h, double value);

/** Calcluate the variance of all counted values. */
double hist_var(const struct hist *h);

/** Calculate the mean average of all counted values. */
double hist_mean(const struct hist *h);

/** Calculate the standard derivation of all counted values. */
double hist_stddev(const struct hist *h);

/** Print all statistical properties of distribution including a graphilcal plot of the histogram. */
void hist_print(const struct hist *h, int details);

/** Print ASCII style plot of histogram */
void hist_plot(const struct hist *h);

/** Dump histogram data in Matlab format.
 *
 * @return The string containing the dump. The caller is responsible to free() the buffer.
 */
char * hist_dump(const struct hist *h);

/** Prints Matlab struct containing all infos to file. */
int hist_dump_matlab(const struct hist *h, FILE *f);

/** Write the histogram in JSON format to fiel \p f. */
int hist_dump_json(const struct hist *h, FILE *f);

/** Build a libjansson / JSON object of the histogram. */
json_t * hist_json(const struct hist *h);

#ifdef __cplusplus
}
#endif
