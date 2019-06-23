/** Histogram class.
 *
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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <float.h>
#include <cmath>
#include <ctime>

#include <villas/utils.hpp>
#include <villas/hist.hpp>
#include <villas/config.h>
#include <villas/table.hpp>

using namespace villas::utils;

namespace villas {

Hist::Hist(int buckets, Hist::cnt_t wu)
{
	length = buckets;
	warmup = wu;

	data = (Hist::cnt_t *) (buckets ? alloc(length * sizeof(Hist::cnt_t)) : nullptr);

	Hist::reset();
}

Hist::~Hist()
{
	if (data)
		free(data);
}

void Hist::put(double value)
{
	last = value;

	/* Update min/max */
	if (value > highest)
		highest = value;
	if (value < lowest)
		lowest = value;

	if (total < warmup) {

	}
	else if (total == warmup) {
		low  = getMean() - 3 * getStddev();
		high = getMean() + 3 * getStddev();
		resolution = (high - low) / length;
	}
	else {
		int idx = round((value - low) / resolution);

		/* Check bounds and increment */
		if      (idx >= length)
			higher++;
		else if (idx < 0)
			lower++;
		else if (data != nullptr)
			data[idx]++;
	}

	total++;

	/* Online / running calculation of variance and mean
	 *  by Donald Knuth’s Art of Computer Programming, Vol 2, page 232, 3rd edition */
	if (total == 1) {
		_m[1] = _m[0] = value;
		_s[1] = 0.0;
	}
	else {
		_m[0] = _m[1] + (value - _m[1]) / total;
		_s[0] = _s[1] + (value - _m[1]) * (value - _m[0]);

		/* Set up for next iteration */
		_m[1] = _m[0];
		_s[1] = _s[0];
	}

}

void Hist::reset()
{
	total = 0;
	higher = 0;
	lower = 0;

	highest = -DBL_MAX;
	lowest = DBL_MAX;

	if (data)
		memset(data, 0, length * sizeof(unsigned));
}

double Hist::getMean() const
{
	return (total > 0) ? _m[0] : NAN;
}

double Hist::getVar() const
{
	return (total > 1) ? _s[0] / (total - 1) : NAN;
}

double Hist::getStddev() const
{
	return sqrt(getVar());
}

void Hist::print(bool details) const
{
	if (total > 0) {
		Hist::cnt_t missed = total - higher - lower;

		info("Counted values: %ju (%ju between %f and %f)", total, missed, low, high);
		info("Highest:  %g", highest);
		info("Lowest:   %g", lowest);
		info("Mu:       %g", getMean());
		info("1/Mu:     %g", 1.0 / getMean());
		info("Variance: %g", getVar());
		info("Stddev:   %g", getStddev());

		if (details && total - higher - lower > 0) {
			char *buf =dump();
			info("Matlab: %s", buf);
			free(buf);

			plot();
		}
	}
	else
		info("Counted values: %ju", total);
}

void Hist::plot() const
{
	Hist::cnt_t max = 1;

	/* Get highest bar */
	for (int i = 0; i < length; i++) {
		if (data[i] > max)
			max = data[i];
	}

	std::vector<TableColumn> cols = {
		{ -9, TableColumn::Alignment::RIGHT, "Value", "%+9.3g" },
		{ -6, TableColumn::Alignment::RIGHT, "Count", "%6ju" },
		{  0, TableColumn::Alignment::LEFT,  "Plot",  "%s", "occurences" }
	};

	Table table = Table(cols);

	/* Print plot */
	table.header();

	for (int i = 0; i < length; i++) {
		double value = low + (i) * resolution;
		Hist::cnt_t cnt = data[i];
		int bar = cols[2].getWidth() * ((double) cnt / max);

		char *buf = strf("%s", "");
		for (int i = 0; i < bar; i++)
			buf = strcatf(&buf, "\u2588");

		table.row(3, value, cnt, buf);

		free(buf);
	}
}

char * Hist::dump() const
{
	char *buf = (char *) alloc(128);

	strcatf(&buf, "[ ");

	for (int i = 0; i < length; i++)
		strcatf(&buf, "%ju ", data[i]);

	strcatf(&buf, "]");

	return buf;
}

json_t * Hist::toJson() const
{
	json_t *json_buckets, *json_hist;

	json_hist = json_pack("{ s: f, s: f, s: i }",
		"low", low,
		"high", high,
		"total", total
	);

	if (total > 0) {
		json_object_update(json_hist, json_pack("{ s: i, s: i, s: f, s: f, s: f, s: f, s: f }",
			"higher", higher,
			"lower", lower,
			"highest", highest,
			"lowest", lowest,
			"mean", getMean(),
			"variance", getVar(),
			"stddev", getStddev()
		));
	}

	if (total - lower - higher > 0) {
		json_buckets = json_array();

		for (int i = 0; i < length; i++)
			json_array_append(json_buckets, json_integer(data[i]));

		json_object_set(json_hist, "buckets", json_buckets);
	}

	return json_hist;
}

int Hist::dumpJson(FILE *f) const
{
	json_t *j = Hist::toJson();

	int ret = json_dumpf(j, f, 0);

	json_decref(j);

	return ret;
}

int Hist::dumpMatlab(FILE *f) const
{
	fprintf(f, "struct(");
	fprintf(f, "'low', %f, ", low);
	fprintf(f, "'high', %f, ", high);
	fprintf(f, "'total', %ju, ", total);
	fprintf(f, "'higher', %ju, ", higher);
	fprintf(f, "'lower', %ju, ", lower);
	fprintf(f, "'highest', %f, ", highest);
	fprintf(f, "'lowest', %f, ", lowest);
	fprintf(f, "'mean', %f, ", getMean());
	fprintf(f, "'variance', %f, ", getVar());
	fprintf(f, "'stddev', %f, ", getStddev());

	if (total - lower - higher > 0) {
		char *buf = dump();
		fprintf(f, "'buckets', %s", buf);
		free(buf);
	}
	else
		fprintf(f, "'buckets', zeros(1, %d)", length);

	fprintf(f, ")");

	return 0;
}

} /* namespace villas */
