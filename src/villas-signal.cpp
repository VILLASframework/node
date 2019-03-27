/** Generate random packages on stdout.
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
 **********************************************************************************/

#include <unistd.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <atomic>

#include <villas/io.h>
#include <villas/utils.hpp>
#include <villas/exceptions.hpp>
#include <villas/copyright.hpp>
#include <villas/log.hpp>
#include <villas/sample.h>
#include <villas/timing.h>
#include <villas/node.h>
#include <villas/pool.h>
#include <villas/plugin.h>
#include <villas/nodes/signal_generator.h>

using namespace villas;

static std::atomic<bool> stop(false);

json_t * parse_cli(int argc, char *argv[])
{
	Logger logger = logging.get("signal");

	/* Default values */
	double rate = 10;
	double frequency = 1;
	double amplitude = 1;
	double stddev = 0.02;
	double offset = 0;
	char *type;
	int rt = 1;
	int values = 1;
	int limit = -1;

	/* Parse optional command line arguments */
	int c;
	char *endptr;
	while ((c = getopt(argc, argv, "v:r:f:l:a:D:no:d:")) != -1) {
		switch (c) {
			case 'n':
				rt = 0;
				break;

			case 'l':
				limit = strtoul(optarg, &endptr, 10);
				goto check;

			case 'v':
				values = strtoul(optarg, &endptr, 10);
				goto check;

			case 'r':
				rate = strtof(optarg, &endptr);
				goto check;

			case 'o':
				offset = strtof(optarg, &endptr);
				goto check;

			case 'f':
				frequency = strtof(optarg, &endptr);
				goto check;

			case 'a':
				amplitude = strtof(optarg, &endptr);
				goto check;

			case 'D':
				stddev = strtof(optarg, &endptr);
				goto check;

			case 'd':
				logging.setLevel(optarg);
				break;

			case '?':
				break;
		}

		continue;

check:		if (optarg == endptr)
			logger->warn("Failed to parse parse option argument '-{} {}'", c, optarg);
	}

	if (argc != optind + 1)
		return nullptr;

	type = argv[optind];

	return json_pack("{ s: s, s: s, s: f, s: f, s: f, s: f, s: f, s: b, s: i, s: i }",
		"type", "signal",
		"signal", type,
		"rate", rate,
		"frequency", frequency,
		"amplitude", amplitude,
		"stddev", stddev,
		"offset", offset,
		"realtime", rt,
		"values", values,
		"limit", limit
	);
}

void usage()
{
	std::cout << "Usage: villas-signal [OPTIONS] SIGNAL" << std::endl
	          << "  SIGNAL   is on of the following signal types:" << std::endl
	          << "    mixed" << std::endl
	          << "    random" << std::endl
	          << "    sine" << std::endl
	          << "    triangle" << std::endl
	          << "    square" << std::endl
	          << "    ramp" << std::endl
	          << "    constants" << std::endl
	          << "    counter" << std::endl << std::endl
	          << "  OPTIONS is one or more of the following options:" << std::endl
	          << "    -d LVL  set debug level" << std::endl
	          << "    -f FMT  set the format" << std::endl
	          << "    -v NUM  specifies how many values a message should contain" << std::endl
	          << "    -r HZ   how many messages per second" << std::endl
	          << "    -n      non real-time mode. do not throttle output." << std::endl
	          << "    -F HZ   the frequency of the signal" << std::endl
	          << "    -a FLT  the amplitude" << std::endl
	          << "    -D FLT  the standard deviation for 'random' signals" << std::endl
	          << "    -o OFF  the DC bias" << std::endl
	          << "    -l NUM  only send LIMIT messages and stop" << std::endl << std::endl;

	print_copyright();
}

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	Logger logger = logging.get("signal");

	switch (signal)  {
		case  SIGALRM:
			logger->info("Reached timeout. Terminating...");
			break;

		default:
			logger->info("Received {} signal. Terminating...", strsignal(signal));
	}

	stop = true;
}

int main(int argc, char *argv[])
{
	int ret;
	json_t *cfg;
	struct node_type *nt;
	struct format_type *ft;

	const char *format = "villas.human"; /** @todo hardcoded for now */

	struct node n;
	n.state = STATE_DESTROYED;
	n.in.state = STATE_DESTROYED;
	n.out.state = STATE_DESTROYED;

	struct io io = { .state = STATE_DESTROYED };
	struct pool q = { .state = STATE_DESTROYED };
	struct sample *t;

	q.queue.state = STATE_DESTROYED;

	Logger logger = logging.get("signal");

	ret = utils::signals_init(quit);
	if (ret)
		throw RuntimeError("Failed to intialize signals");

	ft = format_type_lookup(format);
	if (!ft)
		throw RuntimeError("Invalid output format '{}'", format);

	ret = memory_init(0);
	if (ret)
		throw RuntimeError("Failed to initialize memory");

	nt = node_type_lookup("signal");
	if (!nt)
		throw RuntimeError("Signal generation is not supported.");

	ret = node_init(&n, nt);
	if (ret)
		throw RuntimeError("Failed to initialize node");

	cfg = parse_cli(argc, argv);
	if (!cfg) {
		usage();
		exit(EXIT_FAILURE);
	}

	ret = node_parse(&n, cfg, "cli");
	if (ret) {
		usage();
		exit(EXIT_FAILURE);
	}

	// nt == n._vt
	ret = node_type_start(nt, nullptr);
	if (ret)
		throw RuntimeError("Failed to initialize node type: {}", node_type_name(nt));

	ret = node_check(&n);
	if (ret)
		throw RuntimeError("Failed to verify node configuration");

	ret = pool_init(&q, 16, SAMPLE_LENGTH(vlist_length(&n.in.signals)), &memory_heap);
	if (ret)
		throw RuntimeError("Failed to initialize pool");

	ret = node_prepare(&n);
	if (ret)
		throw RuntimeError("Failed to start node {}: reason={}", node_name(&n), ret);

	ret = node_start(&n);
	if (ret)
		throw RuntimeError("Failed to start node {}: reason={}", node_name(&n), ret);

	ret = io_init(&io, ft, &n.in.signals, IO_FLUSH | (SAMPLE_HAS_ALL & ~SAMPLE_HAS_OFFSET));
	if (ret)
		throw RuntimeError("Failed to initialize output");

	ret = io_check(&io);
	if (ret)
		throw RuntimeError("Failed to validate IO configuration");

	ret = io_open(&io, nullptr);
	if (ret)
		throw RuntimeError("Failed to open output");

	while (!stop && n.state == STATE_STARTED) {
		t = sample_alloc(&q);

		unsigned release = 1; // release = allocated

retry:		ret = node_read(&n, &t, 1, &release);
		if (ret == 0)
			goto retry;
		else if (ret < 0)
			goto out;

		io_print(&io, &t, 1);

out:		sample_decref(t);
	}

	ret = node_stop(&n);
	if (ret)
		throw RuntimeError("Failed to stop node");

	ret = node_destroy(&n);
	if (ret)
		throw RuntimeError("Failed to destroy node");

	ret = io_close(&io);
	if (ret)
		throw RuntimeError("Failed to close IO");

	ret = io_destroy(&io);
	if (ret)
		throw RuntimeError("Failed to destroy IO");

	ret = pool_destroy(&q);
	if (ret)
		throw RuntimeError("Failed to destroy pool");

	logger->info(CLR_GRN("Goodbye!"));

	return 0;
}

/** @} */
