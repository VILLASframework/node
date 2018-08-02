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

#include <iostream>
#include <stdbool.h>
#include <getopt.h>

#include <jansson.h>

#include <villas/sample.h>
#include <villas/io.h>
#include <villas/format_type.h>
#include <villas/utils.h>
#include <villas/pool.h>
#include <villas/config.h>

struct side {
	char *path;
	const char *format;

	struct sample *sample;

	struct io io;
	struct format_type *fmt;
};

void usage()
{
	std::cout << "Usage: villas-test-cmp [OPTIONS] FILE1 FILE2 ... FILEn" << std::endl;
	std::cout << "  FILE     a list of files to compare" << std::endl;
	std::cout << "  OPTIONS is one or more of the following options:" << std::endl;
	std::cout << "    -d LVL  adjust the debug level" << std::endl;
	std::cout << "    -e EPS  set epsilon for floating point comparisons to EPS" << std::endl;
	std::cout << "    -v      ignore data values" << std::endl;
	std::cout << "    -t      ignore timestamp" << std::endl;
	std::cout << "    -s      ignore sequence no" << std::endl;
	std::cout << "    -f FMT  file format for all files" << std::endl;
	std::cout << "    -h      show this usage information" << std::endl;
	std::cout << "    -V      show the version of the tool" << std::endl << std::endl;
	std::cout << "Return codes:" << std::endl;
	std::cout << "  0   files are equal" << std::endl;
	std::cout << "  1   file length not equal" << std::endl;
	std::cout << "  2   sequence no not equal" << std::endl;
	std::cout << "  3   timestamp not equal" << std::endl;
	std::cout << "  4   number of values is not equal" << std::endl;
	std::cout << "  5   data is not equal" << std::endl << std::endl;

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;

	/* Default values */
	double epsilon = 1e-9;
	const char *format = "villas.human";
	int flags = SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_VALUES | SAMPLE_HAS_ORIGIN;

	struct pool pool = { .state = STATE_DESTROYED };

	/* Parse Arguments */
	char c, *endptr;
	while ((c = getopt (argc, argv, "he:vtsf:V")) != -1) {
		switch (c) {
			case 'e':
				epsilon = strtod(optarg, &endptr);
				goto check;
			case 'v':
				flags &= ~SAMPLE_HAS_VALUES;
				break;
			case 't':
				flags &= ~SAMPLE_HAS_ORIGIN;
				break;
			case 's':
				flags &= ~SAMPLE_HAS_SEQUENCE;
				break;
			case 'f':
				format = optarg;
				break;
			case 'V':
				print_version();
				exit(EXIT_SUCCESS);
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

	memory_init(0);
	ret = pool_init(&pool, n, SAMPLE_LENGTH(DEFAULT_SAMPLELEN), &memory_type_heap);
	if (ret)
		error("Failed to initialize pool");

	/* Open files */
	for (int i = 0; i < n; i++) {
		s[i].format = format;
		s[i].path = argv[optind + i];
		s[i].io.state = STATE_DESTROYED;

		s[i].fmt = format_type_lookup(s[i].format);
		if (!s[i].fmt)
			error("Invalid IO format: %s", s[i].format);

		ret = io_init(&s[i].io, s[i].fmt, NULL, 0);
		if (ret)
			error("Failed to initialize IO");

		ret = io_open(&s[i].io, s[i].path);
		if (ret)
			error("Failed to open file: %s", s[i].path);

		s[i].sample = sample_alloc(&pool);
		if (!s[i].sample)
			error("Failed to allocate samples");
	}

	int eofs, line, failed;

	line = 0;
	for (;;) {
		/* Read next sample from all files */
retry:		eofs = 0;
		for (int i = 0; i < n; i++) {
			ret = io_eof(&s[i].io);
			if (ret)
				eofs++;
		}

		if (eofs) {
			if (eofs == n)
				ret = 0;
			else {
				std::cout << "length unequal" << std::endl;
				ret = 1;
			}

			goto out;
		}

		failed = 0;
		for (int i = 0; i < n; i++) {
			ret = io_scan(&s[i].io, &s[i].sample, 1);
			if (ret <= 0)
				failed++;
		}

		if (failed)
			goto retry;

		/* We compare all files against the first one */
		for (int i = 1; i < n; i++) {
			ret = sample_cmp(s[0].sample, s[i].sample, epsilon, flags);
			if (ret)
				goto out;
		}

		line++;
	}

out:	for (int i = 0; i < n; i++) {
		ret = io_close(&s[i].io);
		if (ret)
			error("Failed to close IO");

		ret = io_destroy(&s[i].io);
		if (ret)
			error("Failed to destroy IO");

		sample_put(s[i].sample);
	}

	ret = pool_destroy(&pool);
	if (ret)
		error("Failed to destroy pool");

	return ret;
}
