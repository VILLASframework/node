/** Histogram functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _HIST_H_
#define _HIST_H_

#define HIST_HEIGHT	50
#define HIST_SEQ	17

/** Histogram structure */
typedef unsigned hist_cnt_t;

struct hist {
	double resolution;
	
	double high;
	double low;
	
	double highest;
	double lowest;
	
	int length;

	hist_cnt_t total;
	hist_cnt_t higher;
	hist_cnt_t lower;

	hist_cnt_t *data;
};

void hist_init(struct hist *h, double start, double end, double resolution);

void hist_free(struct hist *h);

void hist_reset(struct hist *h);

void hist_put(struct hist *h, double value);

double hist_var(struct hist *h);

double hist_mean(struct hist *h);

double hist_stddev(struct hist *h);

void hist_print(struct hist *h);

/** Print ASCII style plot of histogram */
void hist_plot(struct hist *h);

/** Dump histogram data in Matlab format */
void hist_dump(struct hist *h);

#endif /* _HIST_H_ */