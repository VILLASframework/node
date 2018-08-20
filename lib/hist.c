/** Histogram functions.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include <time.h>

#include <villas/utils.h>
#include <villas/hist.h>
#include <villas/config.h>
#include <villas/table.h>

#define VAL(h, i)	((h)->low + (i) * (h)->resolution)
#define INDEX(h, v)	round((v - (h)->low) / (h)->resolution)

int hist_init(struct hist *h, int buckets, hist_cnt_t warmup)
{
	h->length = buckets;
	h->warmup = warmup;

	h->data = buckets ? alloc(h->length * sizeof(hist_cnt_t)) : NULL;

	hist_reset(h);

	return 0;
}

int hist_destroy(struct hist *h)
{
	if (h->data) {
		free(h->data);
		h->data = NULL;
	}

	return 0;
}

void hist_put(struct hist *h, double value)
{
	h->last = value;

	/* Update min/max */
	if (value > h->highest)
		h->highest = value;
	if (value < h->lowest)
		h->lowest = value;

	if (h->total < h->warmup) {

	}
	else if (h->total == h->warmup) {
		h->low  = hist_mean(h) - 3 * hist_stddev(h);
		h->high = hist_mean(h) + 3 * hist_stddev(h);
		h->resolution = (h->high - h->low) / h->length;
	}
	else {
		int idx = INDEX(h, value);

		/* Check bounds and increment */
		if      (idx >= h->length)
			h->higher++;
		else if (idx < 0)
			h->lower++;
		else if (h->data != NULL)
			h->data[idx]++;
	}

	h->total++;

	/* Online / running calculation of variance and mean
	 *  by Donald Knuth’s Art of Computer Programming, Vol 2, page 232, 3rd edition */
	if (h->total == 1) {
		h->_m[1] = h->_m[0] = value;
		h->_s[1] = 0.0;
	}
	else {
		h->_m[0] = h->_m[1] + (value - h->_m[1]) / h->total;
		h->_s[0] = h->_s[1] + (value - h->_m[1]) * (value - h->_m[0]);

		// set up for next iteration
		h->_m[1] = h->_m[0];
		h->_s[1] = h->_s[0];
	}

}

void hist_reset(struct hist *h)
{
	h->total = 0;
	h->higher = 0;
	h->lower = 0;

	h->highest = DBL_MIN;
	h->lowest = DBL_MAX;

	if (h->data)
		memset(h->data, 0, h->length * sizeof(unsigned));
}

double hist_mean(const struct hist *h)
{
	return (h->total > 0) ? h->_m[0] : NAN;
}

double hist_var(const struct hist *h)
{
	return (h->total > 1) ? h->_s[0] / (h->total - 1) : NAN;
}

double hist_stddev(const struct hist *h)
{
	return sqrt(hist_var(h));
}

void hist_print(const struct hist *h, int details)
{
	if (h->total > 0) {
		hist_cnt_t missed = h->total - h->higher - h->lower;

		stats("Counted values: %ju (%ju between %f and %f)", h->total, missed, h->low, h->high);
		stats("Highest:  %g", h->highest);
		stats("Lowest:   %g", h->lowest);
		stats("Mu:       %g", hist_mean(h));
		stats("1/Mu:     %g", 1.0/hist_mean(h));
		stats("Variance: %g", hist_var(h));
		stats("Stddev:   %g", hist_stddev(h));

		if (details > 0 && h->total - h->higher - h->lower > 0) {
			char *buf = hist_dump(h);
			stats("Matlab: %s", buf);
			free(buf);

			hist_plot(h);
		}
	}
	else
		stats("Counted values: %ju", h->total);
}

void hist_plot(const struct hist *h)
{
	hist_cnt_t max = 1;

	/* Get highest bar */
	for (int i = 0; i < h->length; i++) {
		if (h->data[i] > max)
			max = h->data[i];
	}

	struct table_column cols[] = {
		{ -9, "Value", "%+9.3g", NULL,         TABLE_ALIGN_RIGHT },
		{ -6, "Count", "%6ju",   NULL,         TABLE_ALIGN_RIGHT },
		{  0, "Plot",  "%s",     "occurences", TABLE_ALIGN_LEFT  }
	};

	struct table table = {
		.ncols = ARRAY_LEN(cols),
		.cols = cols
	};

	/* Print plot */
	table_header(&table);

	for (int i = 0; i < h->length; i++) {
		double value = VAL(h, i);
		hist_cnt_t cnt = h->data[i];
		int bar = cols[2]._width * ((double) cnt / max);

		char *buf = strf("%s", "");
		for (int i = 0; i < bar; i++)
			buf = strcatf(&buf, "\u2588");

		table_row(&table, value, cnt, buf);

		free(buf);
	}

	table_footer(&table);
}

char * hist_dump(const struct hist *h)
{
	char *buf = alloc(128);

	strcatf(&buf, "[ ");

	for (int i = 0; i < h->length; i++)
		strcatf(&buf, "%ju ", h->data[i]);

	strcatf(&buf, "]");

	return buf;
}

json_t * hist_json(const struct hist *h)
{
	json_t *json_buckets, *json_hist;

	json_hist = json_pack("{ s: f, s: f, s: i }",
		"low", h->low,
		"high", h->high,
		"total", h->total
	);

	if (h->total > 0) {
		json_object_update(json_hist, json_pack("{ s: i, s: i, s: f, s: f, s: f, s: f, s: f }",
			"higher", h->higher,
			"lower", h->lower,
			"highest", h->highest,
			"lowest", h->lowest,
			"mean", hist_mean(h),
			"variance", hist_var(h),
			"stddev", hist_stddev(h)
		));
	}

	if (h->total - h->lower - h->higher > 0) {
		json_buckets = json_array();

		for (int i = 0; i < h->length; i++)
			json_array_append(json_buckets, json_integer(h->data[i]));

		json_object_set(json_hist, "buckets", json_buckets);
	}

	return json_hist;
}

int hist_dump_json(const struct hist *h, FILE *f)
{
	json_t *j = hist_json(h);

	int ret = json_dumpf(j, f, 0);

	json_decref(j);

	return ret;
}

int hist_dump_matlab(const struct hist *h, FILE *f)
{
	fprintf(f, "struct(");
	fprintf(f, "'low', %f, ", h->low);
	fprintf(f, "'high', %f, ", h->high);
	fprintf(f, "'total', %ju, ", h->total);
	fprintf(f, "'higher', %ju, ", h->higher);
	fprintf(f, "'lower', %ju, ", h->lower);
	fprintf(f, "'highest', %f, ", h->highest);
	fprintf(f, "'lowest', %f, ", h->lowest);
	fprintf(f, "'mean', %f, ", hist_mean(h));
	fprintf(f, "'variance', %f, ", hist_var(h));
	fprintf(f, "'stddev', %f, ", hist_stddev(h));

	if (h->total - h->lower - h->higher > 0) {
		char *buf = hist_dump(h);
		fprintf(f, "'buckets', %s", buf);
		free(buf);
	}
	else
		fprintf(f, "'buckets', zeros(1, %d)", h->length);

	fprintf(f, ")");

	return 0;
}
