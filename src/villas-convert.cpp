/** Convert between samples IO formats
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
 *
 * @addtogroup tools Test and debug tools
 * @{
 *********************************************************************************/

#include <iostream>

#include <villas/tool.hpp>
#include <villas/utils.hpp>
#include <villas/log.hpp>
#include <villas/io.h>
#include <villas/sample.h>
#include <villas/plugin.h>
#include <villas/exceptions.hpp>

using namespace villas;

namespace villas {
namespace node {
namespace tools {

class Convert : public Tool {

public:
	Convert(int argc, char *argv[]) :
		Tool(argc, argv, "convert"),
		dtypes("64f")
	{
		int ret;

		ret = memory_init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");

		for (unsigned i = 0; i < ARRAY_LEN(dirs); i++) {
			dirs[i].name = i == 0 ? "in" : "out";
			dirs[i].format = "villas.human";
			dirs[i].io.state = State::DESTROYED;
		}
	}

protected:
	std::string dtypes;

	struct {
		std::string name;
		std::string format;
		struct io io;
	} dirs[2];

	void usage()
	{
		std::cout << "Usage: villas-convert [OPTIONS]" << std::endl
			<< "  OPTIONS are:" << std::endl
			<< "    -i FMT           set the input format" << std::endl
			<< "    -o FMT           set the output format" << std::endl
			<< "    -t DT            the data-type format string" << std::endl
			<< "    -d LVL           set debug log level to LVL" << std::endl
			<< "    -h               show this usage information" << std::endl
			<< "    -V               show the version of the tool" << std::endl << std::endl;

		printCopyright();
	}

	void parse()
	{
		/* Parse optional command line arguments */
		int c;
		while ((c = getopt(argc, argv, "Vhd:i:o:t:")) != -1) {
			switch (c) {
				case 'V':
					printVersion();
					exit(EXIT_SUCCESS);

				case 'i':
					dirs[0].format = optarg;
					break;

				case 'o':
					dirs[1].format = optarg;
					break;

				case 't':
					dtypes = optarg;
					break;

				case 'd':
					logging.setLevel(optarg);
					break;

				case 'h':
				case '?':
					usage();
					exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
			}
		}

		if (argc != optind) {
			usage();
			exit(EXIT_FAILURE);
		}
	}

	int main()
	{
		int ret;

		for (unsigned i = 0; i < ARRAY_LEN(dirs); i++) {
			struct format_type *ft;

			ft = format_type_lookup(dirs[i].format.c_str());
			if (!ft)
				throw RuntimeError("Invalid format: {}", dirs[i].format);

			ret = io_init2(&dirs[i].io, ft, dtypes.c_str(), (int) SampleFlags::HAS_ALL);
			if (ret)
				throw RuntimeError("Failed to initialize IO: {}", dirs[i].name);

			ret = io_check(&dirs[i].io);
			if (ret)
				throw RuntimeError("Failed to validate IO configuration");

			ret = io_open(&dirs[i].io, nullptr);
			if (ret)
				throw RuntimeError("Failed to open IO");
		}

		struct sample *smp = sample_alloc_mem(DEFAULT_SAMPLE_LENGTH);

		for (;;) {
			ret = io_scan(&dirs[0].io, &smp, 1);
			if (ret == 0)
				continue;
			if (ret < 0)
				break;

			io_print(&dirs[1].io, &smp, 1);
		}

		for (unsigned i = 0; i < ARRAY_LEN(dirs); i++) {
			ret = io_close(&dirs[i].io);
			if (ret)
				throw RuntimeError("Failed to close IO");

			ret = io_destroy(&dirs[i].io);
			if (ret)
				throw RuntimeError("Failed to destroy IO");
		}

		return 0;
	}
};

} // namespace tools
} // namespace node
} // namespace villas

int main(int argc, char *argv[])
{
	villas::node::tools::Convert t(argc, argv);

	return t.run();
}

/** @} */
