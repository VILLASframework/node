/** Generate random packages on stdout.
 *
 * @file
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
 *
 * @addtogroup tools Test and debug tools
 * @{
 **********************************************************************************/

#include <unistd.h>
#include <math.h>
#include <string.h>
#include <iostream>

#include <villas/io.h>
#include <villas/utils.h>
#include <villas/sample.h>
#include <villas/timing.h>
#include <villas/node.h>
#include <villas/pool.h>
#include <villas/plugin.h>
#include <villas/nodes/signal_generator.h>

/* Some default values */
struct node n;
struct log l;
struct io io;
struct pool q;
struct sample *t;

void usage()
{
	std::cout << "Usage: villas-signal [OPTIONS] SIGNAL" << std::endl;
	std::cout << "  SIGNAL   is on of the following signal types:" << std::endl;
	std::cout << "    mixed" << std::endl;
	std::cout << "    random" << std::endl;
	std::cout << "    sine" << std::endl;
	std::cout << "    triangle" << std::endl;
	std::cout << "    square" << std::endl;
	std::cout << "    ramp" << std::endl;
	std::cout << "    constants" << std::endl;
	std::cout << "    counter" << std::endl;
	std::cout << "" << std::endl;
	std::cout << "  OPTIONS is one or more of the following options:" << std::endl;
	std::cout << "    -d LVL  set debug level" << std::endl;
	std::cout << "    -f FMT  set the format" << std::endl;
	std::cout << "    -v NUM  specifies how many values a message should contain" << std::endl;
	std::cout << "    -r HZ   how many messages per second" << std::endl;
	std::cout << "    -n      non real-time mode. do not throttle output." << std::endl;
	std::cout << "    -F HZ   the frequency of the signal" << std::endl;
	std::cout << "    -a FLT  the amplitude" << std::endl;
	std::cout << "    -D FLT  the standard deviation for 'random' signals" << std::endl;
	std::cout << "    -o OFF  the DC bias" << std::endl;
	std::cout << "    -l NUM  only send LIMIT messages and stop" << std::endl << std::endl;

	print_copyright();
}

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	int ret;

	ret = node_stop(&n);
	if (ret)
		error("Failed to stop node");

	ret = node_destroy(&n);
	if (ret)
		error("Failed to destroy node");

	ret = io_close(&io);
	if (ret)
		error("Failed to close IO");

	ret = io_destroy(&io);
	if (ret)
		error("Failed to destroy IO");

	ret = pool_destroy(&q);
	if (ret)
		error("Failed to destroy pool");

	ret = log_close(&l);
	if (ret)
		error("Failed to stop log");

	info(CLR_GRN("Goodbye!"));
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	int ret;
	struct node_type *nt;
	struct format_type *ft;

	const char *format = "villas.human"; /** @todo hardcoded for now */

	ret = log_init(&l, l.level, LOG_ALL);
	if (ret)
		error("Failed to initialize log");

	ret = log_parse_wrapper(&l, n.cfg);
	if (ret)
		error("Failed to parse log");

	ret = log_open(&l);
	if (ret)
		error("Failed to start log");

	ret = signals_init(quit);
	if (ret)
		error("Failed to intialize signals");

	nt = node_type_lookup("signal");
	if (!nt)
		error("Signal generation is not supported.");

	ret = node_init(&n, nt);
	if (ret)
		error("Failed to initialize node");

	ft = format_type_lookup(format);
	if (!ft)
		error("Invalid output format '%s'", format);

	ret = io_init(&io, ft, NULL, IO_FLUSH | (SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET));
	if (ret)
		error("Failed to initialize output");

	ret = io_open(&io, NULL);
	if (ret)
		error("Failed to open output");

	ret = node_parse_cli(&n, argc, argv);
	if (ret) {
		usage();
		exit(EXIT_FAILURE);
	}

	ret = node_check(&n);
	if (ret)
		error("Failed to verify node configuration");

	ret = pool_init(&q, 16, SAMPLE_LEN(n.samplelen), &memory_type_heap);
	if (ret)
		error("Failed to initialize pool");

	ret = node_start(&n);
	if (ret)
		serror("Failed to start node");

	for (;;) {
		t = sample_alloc(&q);

		node_read(&n, &t, 1);
		io_print(&io, &t, 1);

		sample_put(t);
	}

	return 0;
}

/** @} */
