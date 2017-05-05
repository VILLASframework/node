/** Compare two data files.
 *
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

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <getopt.h>

#include <jansson.h>

#include <villas/sample.h>
#include <villas/sample_io.h>
#include <villas/utils.h>
#include <villas/timing.h>
#include <villas/pool.h>

#include "config.h"

void usage()
{
	printf("Usage: villas-test-cmp [OPTIONS] FILE1 FILE2\n");
	printf("  FILE1    first file to compare\n");
	printf("  FILE2    second file to compare against\n");
	printf("  OPTIONS is one or more of the following options:\n");
	printf("    -h      print this usage information\n");
	printf("    -d LVL  adjust the debug level\n");
	printf("    -e EPS  set epsilon for floating point comparisons to EPS\n");
	printf("\n");
	printf("Return codes:\n");
	printf("  0   files are equal\n");
	printf("  1   file length not equal\n");
	printf("  2   sequence no not equal\n");
	printf("  3   timestamp not equal\n");
	printf("  4   number of values is not equal\n");
	printf("  5   data is not equal\n");
	printf("\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;
	double epsilon = 1e-9;

	struct log log;
	struct pool pool = { .state = STATE_DESTROYED };
	struct sample *samples[2];

	struct {
		int flags;
		char *path;
		FILE *handle;
		struct sample *sample;
	} f1, f2;

	/* Parse Arguments */
	char c, *endptr;
	while ((c = getopt (argc, argv, "hjmd:e:l:H:r:")) != -1) {
		switch (c) {
			case 'd':
				log.level = strtoul(optarg, &endptr, 10);
				goto check;
			case 'e':
				epsilon = strtod(optarg, &endptr);
				goto check;
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}

	if (argc != optind + 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	f1.path = argv[optind];
	f2.path = argv[optind + 1];

	log_init(&log, V, LOG_ALL);
	log_start(&log);

	pool_init(&pool, 1024, SAMPLE_LEN(DEFAULT_SAMPLELEN), &memtype_heap);
	sample_alloc(&pool, samples, 2);

	f1.sample = samples[0];
	f2.sample = samples[1];

	f1.handle = fopen(f1.path, "r");
	if (!f1.handle)
		serror("Failed to open file: %s", f1.path);

	f2.handle = fopen(f2.path, "r");
	if (!f2.handle)
		serror("Failed to open file: %s", f2.path);

	while (!feof(f1.handle) && !feof(f2.handle)) {
		ret = sample_io_villas_fscan(f1.handle, f1.sample, &f1.flags);
		if (ret < 0 && !feof(f1.handle))
			goto out;

		ret = sample_io_villas_fscan(f2.handle, f2.sample, &f2.flags);
		if (ret < 0 && !feof(f2.handle))
			goto out;

		/* EOF is only okay if both files are at the end */
		if (feof(f1.handle) || feof(f2.handle)) {
			if (!(feof(f1.handle) && feof(f2.handle))) {
				printf("file length not equal\n");
				ret = 1;
				goto out;
			}
		}

		/* Compare sequence no */
		if ((f1.flags & SAMPLE_IO_SEQUENCE) && (f2.flags & SAMPLE_IO_SEQUENCE)) {
			if (f1.sample->sequence != f2.sample->sequence) {
				printf("sequence no: %d != %d\n", f1.sample->sequence, f2.sample->sequence);
				ret = 2;
				goto out;
			}
		}

		/* Compare timestamp */
		if (time_delta(&f1.sample->ts.origin, &f2.sample->ts.origin) > epsilon) {
			printf("ts.origin: %f != %f\n", time_to_double(&f1.sample->ts.origin), time_to_double(&f2.sample->ts.origin));
			ret = 3;
			goto out;
		}

		/* Compare data */
		if (f1.sample->length != f2.sample->length) {
			printf("length: %d != %d\n", f1.sample->length, f2.sample->length);
			ret = 4;
			goto out;
		}

		for (int i = 0; i < f1.sample->length; i++) {
			if (fabs(f1.sample->data[i].f - f2.sample->data[i].f) > epsilon) {
				printf("data[%d]: %f != %f\n", i, f1.sample->data[i].f, f2.sample->data[i].f);
				ret = 5;
				goto out;
			}
		}
	}

	ret = 0;

out:	sample_free(samples, 2);

	fclose(f1.handle);
	fclose(f2.handle);

	pool_destroy(&pool);

	return ret;
}