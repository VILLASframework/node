/** Compare two data files.
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

#include <iostream>
#include <stdbool.h>
#include <getopt.h>

#include <jansson.h>

#include <villas/sample.h>
#include <villas/io.h>
#include <villas/format_type.h>
#include <villas/utils.hpp>
#include <villas/log.hpp>
#include <villas/pool.h>
#include <villas/exceptions.hpp>
#include <villas/node/config.h>

using namespace villas;

class Side {

public:
	std::string path;

	struct sample *sample;

	struct io io;
	struct format_type *format;

	Side(const std::string &pth, struct format_type *fmt, struct pool *p) :
		path(pth),
		format(fmt)
	{
		int ret;

		io.state = STATE_DESTROYED;
		ret = io_init_auto(&io, format, DEFAULT_SAMPLE_LENGTH, 0);
		if (ret)
			throw new RuntimeError("Failed to initialize IO");

		ret = io_check(&io);
		if (ret)
			throw new RuntimeError("Failed to validate IO configuration");

		ret = io_open(&io, path.c_str());
		if (ret)
			throw new RuntimeError("Failed to open file: {}", path);

		sample = sample_alloc(p);
		if (!sample)
			throw new RuntimeError("Failed to allocate samples");
	}

	~Side() noexcept(false)
	{
		int ret;

		ret = io_close(&io);
		if (ret)
			throw new RuntimeError("Failed to close IO");

		ret = io_destroy(&io);
		if (ret)
			throw new RuntimeError("Failed to destroy IO");

		sample_decref(sample);
	}
};

void usage()
{
	std::cout << "Usage: villas-test-cmp [OPTIONS] FILE1 FILE2 ... FILEn" << std::endl
	          << "  FILE     a list of files to compare" << std::endl
	          << "  OPTIONS is one or more of the following options:" << std::endl
	          << "    -d LVL  adjust the debug level" << std::endl
	          << "    -e EPS  set epsilon for floating point comparisons to EPS" << std::endl
	          << "    -v      ignore data values" << std::endl
	          << "    -t      ignore timestamp" << std::endl
	          << "    -s      ignore sequence no" << std::endl
	          << "    -f FMT  file format for all files" << std::endl
	          << "    -h      show this usage information" << std::endl
	          << "    -V      show the version of the tool" << std::endl << std::endl
	          << "Return codes:" << std::endl
	          << "  0   files are equal" << std::endl
	          << "  1   file length not equal" << std::endl
	          << "  2   sequence no not equal" << std::endl
	          << "  3   timestamp not equal" << std::endl
	          << "  4   number of values is not equal" << std::endl
	          << "  5   data is not equal" << std::endl << std::endl;

	utils::print_copyright();
}

int main(int argc, char *argv[])
{
	int ret, rc = 0;

	/* Default values */
	double epsilon = 1e-9;
	const char *format = "villas.human";
	int flags = SAMPLE_HAS_SEQUENCE | SAMPLE_HAS_DATA | SAMPLE_HAS_TS_ORIGIN;

	struct pool pool = { .state = STATE_DESTROYED };

	/* Parse Arguments */
	int c;
	char *endptr;
	while ((c = getopt (argc, argv, "he:vtsf:Vd:")) != -1) {
		switch (c) {
			case 'e':
				epsilon = strtod(optarg, &endptr);
				goto check;

			case 'v':
				flags &= ~SAMPLE_HAS_DATA;
				break;

			case 't':
				flags &= ~SAMPLE_HAS_TS_ORIGIN;
				break;

			case 's':
				flags &= ~SAMPLE_HAS_SEQUENCE;
				break;

			case 'f':
				format = optarg;
				break;

			case 'V':
				utils::print_version();
				exit(EXIT_SUCCESS);

			case 'd':
				logging.setLevel(optarg);
				break;

			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr)
			throw new RuntimeError("Failed to parse parse option argument '-{} {}'", c, optarg);
	}

	if (argc - optind < 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	int eofs, line, failed;
	int n = argc - optind; /* The number of files which we compare */
	Side *s[n];

	ret = memory_init(0);
	if (ret)
		throw new RuntimeError("Failed to initialize memory system");

	ret = pool_init(&pool, n, SAMPLE_LENGTH(DEFAULT_SAMPLE_LENGTH), &memory_heap);
	if (ret)
		throw new RuntimeError("Failed to initialize pool");

	struct format_type *fmt = format_type_lookup(format);
	if (!fmt)
		throw new RuntimeError("Invalid IO format: {}", format);

	/* Open files */
	for (int i = 0; i < n; i++)
		s[i] = new Side(argv[optind + i], fmt, &pool);

	line = 0;
	for (;;) {
		/* Read next sample from all files */
retry:		eofs = 0;
		for (int i = 0; i < n; i++) {
			ret = io_eof(&s[i]->io);
			if (ret)
				eofs++;
		}

		if (eofs) {
			if (eofs == n)
				ret = 0;
			else {
				std::cout << "length unequal" << std::endl;
				rc = 1;
			}

			goto out;
		}

		failed = 0;
		for (int i = 0; i < n; i++) {
			ret = io_scan(&s[i]->io, &s[i]->sample, 1);
			if (ret <= 0)
				failed++;
		}
		if (failed)
			goto retry;

		/* We compare all files against the first one */
		for (int i = 1; i < n; i++) {
			ret = sample_cmp(s[0]->sample, s[i]->sample, epsilon, flags);
			if (ret) {
				rc = ret;
				goto out;
			}
		}

		line++;
	}

out:	for (int i = 0; i < n; i++)
		delete s[i];

	ret = pool_destroy(&pool);
	if (ret)
		throw new RuntimeError("Failed to destroy pool");

	return rc;
}
