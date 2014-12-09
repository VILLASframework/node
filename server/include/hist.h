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
struct hist {
	double start;
	double end;
	double resolution;
	int length;
	unsigned *data;
};

void hist_init(struct hist *h, double start, double end, double resolution);

void hist_free(struct hist *h);

void hist_reset(struct hist *h);

void hist_put(struct hist *h, double value);

/** Print ASCII style plot of histogram */
void hist_plot(struct hist *h);

/** Dump histogram data in Matlab format */
void hist_dump(struct hist *h);

#endif /* _HIST_H_ */