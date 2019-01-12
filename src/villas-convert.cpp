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

#include <villas/utils.hpp>
#include <villas/log.hpp>
#include <villas/io.h>
#include <villas/sample.h>
#include <villas/plugin.h>
#include <villas/exceptions.hpp>
#include <villas/copyright.hpp>

using namespace villas;

static void usage()
{
	std::cout << "Usage: villas-convert [OPTIONS]" << std::endl
	          << "  OPTIONS are:" << std::endl
	          << "    -i FMT           set the input format" << std::endl
	          << "    -o FMT           set the output format" << std::endl
	          << "    -d LVL           set debug log level to LVL" << std::endl
	          << "    -h               show this usage information" << std::endl
	          << "    -V               show the version of the tool" << std::endl << std::endl;

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;
	const char *input_format = "villas.human";
	const char *output_format = "villas.human";

	/* Parse optional command line arguments */
	int c;
	while ((c = getopt(argc, argv, "Vhd:i:o:")) != -1) {
		switch (c) {
			case 'V':
				print_version();
				exit(EXIT_SUCCESS);

			case 'i':
				input_format = optarg;
				break;

			case 'o':
				output_format = optarg;
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

	struct format_type *fmt;
	struct io input = { .state = STATE_DESTROYED };
	struct io output = { .state = STATE_DESTROYED };

	struct {
		const char *name;
		struct io *io;
	} dirs[] = {
		{ input_format, &input },
		{ output_format, &output },
	};

	for (unsigned i = 0; i < ARRAY_LEN(dirs); i++) {
		fmt = format_type_lookup(dirs[i].name);
		if (!fmt)
			throw RuntimeError("Invalid format: {}", dirs[i].name);

		ret = io_init_auto(dirs[i].io, fmt, DEFAULT_SAMPLE_LENGTH, SAMPLE_HAS_ALL);
		if (ret)
			throw RuntimeError("Failed to initialize IO: {}", dirs[i].name);

		ret = io_check(dirs[i].io);
		if (ret)
			throw RuntimeError("Failed to validate IO configuration");

		ret = io_open(dirs[i].io, nullptr);
		if (ret)
			throw RuntimeError("Failed to open IO");
	}

	struct sample *smp = sample_alloc_mem(DEFAULT_SAMPLE_LENGTH);

	for (;;) {
		ret = io_scan(&input, &smp, 1);
		if (ret == 0)
			continue;
		if (ret < 0)
			break;

		io_print(&output, &smp, 1);
	}

	for (unsigned i = 0; i < ARRAY_LEN(dirs); i++) {
		ret = io_close(dirs[i].io);
		if (ret)
			throw RuntimeError("Failed to close IO");

		ret = io_destroy(dirs[i].io);
		if (ret)
			throw RuntimeError("Failed to destroy IO");
	}

	return 0;
}

/** @} */
