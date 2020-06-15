/** Compare two data files.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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
#include <getopt.h>

#include <jansson.h>

#include <villas/tool.hpp>
#include <villas/sample.h>
#include <villas/io.h>
#include <villas/format_type.h>
#include <villas/utils.hpp>
#include <villas/log.hpp>
#include <villas/pool.h>
#include <villas/exceptions.hpp>
#include <villas/node/config.h>

using namespace villas;

namespace villas {
namespace node {
namespace tools {

class TestCmpSide {

public:
	std::string path;
	std::string dtypes;

	struct sample *sample;

	struct io io;
	struct format_type *format;

	TestCmpSide(const std::string &pth, struct format_type *fmt, const std::string &dt, struct pool *p) :
		path(pth),
		dtypes(dt),
		format(fmt)
	{
		int ret;

		io.state = State::DESTROYED;
		ret = io_init2(&io, format, dtypes.c_str(), 0);
		if (ret)
			throw RuntimeError("Failed to initialize IO");

		ret = io_check(&io);
		if (ret)
			throw RuntimeError("Failed to validate IO configuration");

		ret = io_open(&io, path.c_str());
		if (ret)
			throw RuntimeError("Failed to open file: {}", path);

		sample = sample_alloc(p);
		if (!sample)
			throw RuntimeError("Failed to allocate samples");
	}

	~TestCmpSide() noexcept(false)
	{
		int ret;

		ret = io_close(&io);
		if (ret)
			throw RuntimeError("Failed to close IO");

		ret = io_destroy(&io);
		if (ret)
			throw RuntimeError("Failed to destroy IO");

		sample_decref(sample);
	}
};

class TestCmp : public Tool {

public:
	TestCmp(int argc, char *argv[]) :
		Tool(argc, argv, "test-cmp"),
		epsilon(1e-9),
		format("villas.human"),
		dtypes("64f"),
		flags((int) SampleFlags::HAS_SEQUENCE | (int) SampleFlags::HAS_DATA | (int) SampleFlags::HAS_TS_ORIGIN)
	{
		pool.state = State::DESTROYED;

		int ret;

		ret = memory_init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");
	}

protected:
	struct pool pool;

	double epsilon;
	std::string format;
	std::string dtypes;
	int flags;

	std::vector<std::string> filenames;

	void usage()
	{
		std::cout << "Usage: villas-test-cmp [OPTIONS] FILE1 FILE2 ... FILEn" << std::endl
			<< "  FILE     a list of files to compare" << std::endl
			<< "  OPTIONS is one or more of the following options:" << std::endl
			<< "    -d LVL  adjust the debug level" << std::endl
			<< "    -e EPS  set epsilon for floating point comparisons to EPS" << std::endl
			<< "    -v      ignore data values" << std::endl
			<< "    -T      ignore timestamp" << std::endl
			<< "    -s      ignore sequence no" << std::endl
			<< "    -f FMT  file format for all files" << std::endl
			<< "    -t DT   the data-type format string" << std::endl
			<< "    -h      show this usage information" << std::endl
			<< "    -V      show the version of the tool" << std::endl << std::endl
			<< "Return codes:" << std::endl
			<< "  0   files are equal" << std::endl
			<< "  1   file length not equal" << std::endl
			<< "  2   sequence no not equal" << std::endl
			<< "  3   timestamp not equal" << std::endl
			<< "  4   number of values is not equal" << std::endl
			<< "  5   data is not equal" << std::endl << std::endl;

		printCopyright();
	}

	void parse()
	{
		/* Parse Arguments */
		int c;
		char *endptr;
		while ((c = getopt (argc, argv, "he:vTsf:t:Vd:")) != -1) {
			switch (c) {
				case 'e':
					epsilon = strtod(optarg, &endptr);
					goto check;

				case 'v':
					flags &= ~(int) SampleFlags::HAS_DATA;
					break;

				case 'T':
					flags &= ~(int) SampleFlags::HAS_TS_ORIGIN;
					break;

				case 's':
					flags &= ~(int) SampleFlags::HAS_SEQUENCE;
					break;

				case 'f':
					format = optarg;
					break;

				case 't':
					dtypes = optarg;
					break;

				case 'V':
					printVersion();
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

check:			if (optarg == endptr)
				throw RuntimeError("Failed to parse parse option argument '-{} {}'", c, optarg);
		}

		if (argc - optind < 2) {
			usage();
			exit(EXIT_FAILURE);
		}

		/* Open files */
		for (int i = 0; i < argc - optind; i++)
			filenames.push_back(argv[optind + i]);
	}

	int main()
	{
		int ret, rc = 0, line, failed;
		unsigned eofs;

		struct format_type *fmt = format_type_lookup(format.c_str());
		if (!fmt)
			throw RuntimeError("Invalid IO format: {}", format);

		ret = pool_init(&pool, filenames.size(), SAMPLE_LENGTH(DEFAULT_SAMPLE_LENGTH), &memory_heap);
		if (ret)
			throw RuntimeError("Failed to initialize pool");

		/* Open files */
		std::vector<TestCmpSide *> sides;
		for (auto filename : filenames)
			sides.push_back(new TestCmpSide(filename, fmt, dtypes, &pool));

		line = 0;
		for (;;) {
			/* Read next sample from all files */
retry:			eofs = 0;
			for (auto side : sides) {
				ret = io_eof(&side->io);
				if (ret)
					eofs++;
			}

			if (eofs) {
				if (eofs == sides.size())
					ret = 0;
				else {
					std::cout << "length unequal" << std::endl;
					rc = 1;
				}

				goto out;
			}

			failed = 0;
			for (auto side : sides) {
				ret = io_scan(&side->io, &side->sample, 1);
				if (ret <= 0)
					failed++;
			}
			if (failed)
				goto retry;

			/* We compare all files against the first one */
			for (auto side : sides) {
				ret = sample_cmp(sides[0]->sample, side->sample, epsilon, flags);
				if (ret) {
					rc = ret;
					goto out;
				}
			}

			line++;
		}

out:		for (auto side : sides)
			delete side;

		ret = pool_destroy(&pool);
		if (ret)
			throw RuntimeError("Failed to destroy pool");

		return rc;
	}
};

} /* namespace tools */
} /* namespace node */
} /* namespace villas */

int main(int argc, char *argv[])
{
	villas::node::tools::TestCmp t(argc, argv);

	return t.run();
}
