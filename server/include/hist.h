/** Histogram functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 *********************************************************************************/
 * @file
 */

#ifndef _HIST_H_
#define _HIST_H_

#include <stdio.h>

#define HIST_HEIGHT	75
#define HIST_SEQ	17

typedef unsigned hist_cnt_t;

/** Histogram structure used to collect statistics. */
struct hist {
	/** The distance between two adjacent buckets. */
	double resolution;
	
	/** The value of the highest bucket. */
	double high;
	/** The value of the lowest bucket. */
	double low;
	
	/** The highest value observed (may be higher than #high). */
	double highest;
	/** The lowest value observed (may be lower than #low). */
	double lowest;
	
	/** The number of buckets in #data. */
	int length;

	/** Total number of counted values between #low and #high. */
	hist_cnt_t total;
	/** The number of values which are higher than #high. */ 
	hist_cnt_t higher;
	/** The number of values which are lower than #low. */
	hist_cnt_t lower;

	/** Pointer to dynamically allocated array of size length. */
	hist_cnt_t *data;
	
	/** Private variables for online variance calculation */
	double _m[2], _s[2];
};

/** Initialize struct hist with supplied values and allocate memory for buckets. */
void hist_create(struct hist *h, double start, double end, double resolution);

/** Free the dynamically allocated memory. */
void hist_destroy(struct hist *h);

/** Reset all counters and values back to zero. */
void hist_reset(struct hist *h);

/** Count a value within its corresponding bucket. */
void hist_put(struct hist *h, double value);

/** Calcluate the variance of all counted values. */
double hist_var(struct hist *h);

/** Calculate the mean average of all counted values. */
double hist_mean(struct hist *h);

/** Calculate the standard derivation of all counted values. */
double hist_stddev(struct hist *h);

/** Print all statistical properties of distribution including a graphilcal plot of the histogram. */
void hist_print(struct hist *h);

/** Print ASCII style plot of histogram */
void hist_plot(struct hist *h);

/** Dump histogram data in Matlab format to buf */
void hist_dump(struct hist *h, char *buf, int len);

/** Prints Matlab struct containing all infos to file. */
void hist_matlab(struct hist *h, FILE *f);

#endif /* _HIST_H_ */
