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
#include <getopt.h>

#include <jansson.h>

#include <villas/sample.h>
#include <villas/io.h>
#include <villas/io_format.h>
#include <villas/utils.h>
#include <villas/pool.h>

#include "config.h"

struct side {
	char *path;
	char *format;

	struct sample *sample;

	struct io io;
	struct io_format *fmt;
};

void usage()
{
	printf("Usage: villas-test-cmp [OPTIONS] FILE1 FILE2 ... FILEn\n");
	printf("  FILE1    first file to compare\n");
	printf("  FILE2    second file to compare against\n");
	printf("  OPTIONS is one or more of the following options:\n");
	printf("    -h      print this usage information\n");
	printf("    -d LVL  adjust the debug level\n");
	printf("    -e EPS  set epsilon for floating point comparisons to EPS\n");
	printf("    -v      ignore data values\n");
	printf("    -t      ignore timestamp\n");
	printf("    -s      ignore sequence no\n");
	printf("    -f      file format for all files\n");
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

	/* Default values */
	double epsilon = 1e-9;
	char *format = "villas-human";
	int flags = SAMPLE_SEQUENCE | SAMPLE_VALUES | SAMPLE_ORIGIN;

	struct pool pool = { .state = STATE_DESTROYED };

	/* Parse Arguments */
	char c, *endptr;
	while ((c = getopt (argc, argv, "he:vtsf:")) != -1) {
		switch (c) {
			case 'e':
				epsilon = strtod(optarg, &endptr);
				goto check;
			case 'v':
				flags &= ~SAMPLE_VALUES;
				break;
			case 't':
				flags &= ~SAMPLE_ORIGIN;
				break;
			case 's':
				flags &= ~SAMPLE_SEQUENCE;
				break;
			case 'f':
				format = optarg;
				break;
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}

	if (argc - optind < 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	int n = argc - optind; /* The number of files which we compare */
	struct side s[n];

	ret = pool_init(&pool, n, SAMPLE_LEN(DEFAULT_SAMPLELEN), &memtype_heap);
	if (ret)
		error("Failed to initialize pool");

	/* Open files */
	for (int i = 0; i < n; i++) {
		s[i].format = format;
		s[i].path = argv[optind + i];

		s[i].fmt = io_format_lookup(s[i].format);
		if (!s[i].fmt)
			error("Invalid IO format: %s", s[i].format);

		ret = io_init(&s[i].io, s[i].fmt, IO_NONBLOCK);
		if (ret)
			error("Failed to initialize IO");

		ret = io_open(&s[i].io, s[i].path);
		if (ret)
			error("Failed to open file: %s", s[i].path);

		ret = sample_alloc(&pool, &s[i].sample, 1);
		if (ret != 1)
			error("Failed to allocate samples");
	}

	int line = 0;
	for (;;) {
		/* Read next sample from all files */
		int fails = 0;
		for (int i = 0; i < n; i++) {
			ret = io_eof(&s[i].io);
			if (ret) {
				fails++;
				continue;
			}

			ret = io_scan(&s[i].io, &s[i].sample, 1);
			if (ret <= 0) {
				fails++;
				continue;
			}
		}

		if (fails == n) {
			ret = 0;
			goto fail;
		}
		else if (fails != n) {
			printf("fails = %d at line %d\n", fails, line);
			ret = -1;
			goto fail;
		}

		/* We compare all files against the first one */
		for (int i = 1; i < n; i++) {
			ret = sample_cmp(s[0].sample, s[i].sample, epsilon, flags);
			if (ret)
				goto fail;
		}

		line++;
	}

fail:
	for (int i = 0; i < n; i++) {
		io_close(&s[i].io);
		io_destroy(&s[i].io);
		sample_put(s[i].sample);
	}

	pool_destroy(&pool);

	return ret;
}
