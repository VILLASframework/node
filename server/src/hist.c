/** Histogram functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <float.h>
#include <math.h>

#include "utils.h"
#include "hist.h"

#define VAL(h, i)	((h)->low + (i) * (h)->resolution)
#define INDEX(h, v)	round((v - (h)->low) / (h)->resolution)

void hist_init(struct hist *h, double low, double high, double resolution)
{
	h->low = low;
	h->high = high;
	h->resolution = resolution;
	h->length = (high - low) / resolution;
	h->data = malloc(h->length * sizeof(unsigned));
	
	hist_reset(h);
}

void hist_free(struct hist *h)
{
	free(h->data);
}

void hist_put(struct hist *h, double value)
{
	int idx = INDEX(h, value);
	
	/* Update min/max */
	if (value > h->highest)
		h->highest = value;
	if (value < h->lowest)
		h->lowest = value;
	
	/* Check bounds and increment */
	if      (idx >= h->length)
		h->higher++;
	else if (idx < 0)
		h->lower++;
	else {		
		h->total++;
		h->data[idx]++;
	}
}

void hist_reset(struct hist *h)
{
	h->total = 0;
	h->higher = 0;
	h->lower = 0;
	
	h->highest = DBL_MIN;
	h->lowest = DBL_MAX;
	
	memset(h->data, 0, h->length * sizeof(unsigned));
}

double hist_mean(struct hist *h)
{
	double mean = 0;

	for (int i = 0; i < h->length; i++)
		mean += VAL(h, i) * h->data[i];
	
	return mean / h->total;
}

double hist_var(struct hist *h)
{
	double mean_x2 = 0;
	double mean = hist_mean(h);
	
	for (int i = 0; i < h->length; i++)
		mean_x2 += pow(VAL(h, i), 2) * h->data[i];
	
	/* Var[X] = E[X^2] - E^2[X] */
	return mean_x2 / h->total - pow(mean, 2);
}

double hist_stddev(struct hist *h)
{
	return sqrt(hist_var(h));
}

void hist_print(struct hist *h)
{ INDENT
	info("Total: %u values between %f and %f", h->total, h->low, h->high);
	info("Missed:  %u (above), %u (below) ", h->higher, h->lower);
	info("Highest value: %f, lowest %f", h->highest, h->lowest);
	info("Mean: %f", hist_mean(h));
	info("Variance: %f", hist_var(h));
	info("Standard derivation: %f", hist_stddev(h));
		
	hist_plot(h);
	hist_dump(h);
}

void hist_plot(struct hist *h)
{
	unsigned min = UINT_MAX;
	unsigned max = 0;

	/* Get max, first & last */
	for (int i = 0; i < h->length; i++) {
		if (h->data[i] > max)
			max = h->data[i];
		if (h->data[i] < min)
			min = h->data[i];
	}
	
	char buf[HIST_HEIGHT];
	memset(buf, '#', HIST_HEIGHT);

	/* Print plot */
	info("%5s | %5s | %s", "Value", "Occur", "Histogram Plot:");
	for (int i = 0; i < h->length; i++) {
		int bar = HIST_HEIGHT * ((double) h->data[i] / max);

		if      (h->data[i] == min) info("%5.2e | " GRN("%5u") " | %.*s", VAL(h, i), h->data[i], bar, buf);
		else if (h->data[i] == max) info("%5.2e | " RED("%5u") " | %.*s", VAL(h, i), h->data[i], bar, buf);
		else			    info("%5.2e | "     "%5u"  " | %.*s", VAL(h, i), h->data[i], bar, buf);
	}
}

void hist_dump(struct hist *h)
{
	char tok[8];
	char buf[h->length * sizeof(tok)];
	memset(buf, 0, sizeof(buf));

	/* Print in Matlab vector format */
	for (int i = 0; i < h->length; i++) {
		snprintf(tok, sizeof(tok), "%u ", h->data[i]);
		strncat(buf, tok, sizeof(buf) - strlen(buf));
	}

	info("hist = [ %s]", buf);
}
