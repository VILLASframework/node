/** Histogram functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>

#include "utils.h"
#include "hist.h"

void hist_init(struct hist *h, double start, double end, double resolution)
{
	h->start = start;
	h->end = end;
	h->resolution = resolution;
	h->length = (end - start) / resolution;
	h->data = malloc(h->length * sizeof(unsigned));
	
	hist_reset(h);
}

void hist_free(struct hist *h)
{
	free(h->data);
}

void hist_put(struct hist *h, double value)
{
	int index = round((value - h->start) / h->resolution);
	
	if (index >= 0 && index < h->length)
		h->data[index]++;
}

void hist_reset(struct hist *h)
{
	memset(h->data, 0, h->length * sizeof(unsigned));
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
		double val = h->start + i * h->resolution;
		int bar = HIST_HEIGHT * ((double) h->data[i] / max);

		if      (h->data[i] == min) info("%5.2e | " GRN("%5u") " | %.*s", val, h->data[i], bar, buf);
		else if (h->data[i] == max) info("%5.2e | " RED("%5u") " | %.*s", val, h->data[i], bar, buf);
		else			    info("%5.2e | "     "%5u"  " | %.*s", val, h->data[i], bar, buf);
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
