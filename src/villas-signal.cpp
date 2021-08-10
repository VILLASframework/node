/** Generate random packages on stdout.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
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
 **********************************************************************************/

#include <unistd.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <atomic>

#include <villas/tool.hpp>
#include <villas/format.hpp>
#include <villas/utils.hpp>
#include <villas/colors.hpp>
#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/sample.hpp>
#include <villas/timing.hpp>
#include <villas/node.hpp>
#include <villas/pool.hpp>
#include <villas/nodes/signal.hpp>

using namespace villas;

namespace villas {
namespace node {
namespace tools {

class Signal : public Tool {

public:
	Signal(int argc, char *argv[]) :
		Tool(argc, argv, "signal"),
		stop(false),
		node(),
		formatter(nullptr),
		pool(),
		format("villas.human")
	{
		int ret;

		ret = memory::init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");
	}

protected:
	std::atomic<bool> stop;

	Node *node;
	Format *formatter;
	struct Pool pool;

	std::string format;

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

		printCopyright();
	}

	json_t * parse_cli(int argc, char *argv[])
	{
		/* Default values */
		double rate = 10;
		double frequency = 1;
		double amplitude = 1;
		double stddev = 0.02;
		double offset = 0;
		double phase = 0.0;
		double pulse_low = 0.0;
		double pulse_high = 1.0;
		double pulse_width = 1.0;

		std::string type;
		int rt = 1;
		int values = 1;
		int limit = -1;

		/* Parse optional command line arguments */
		int c;
		char *endptr;
		while ((c = getopt(argc, argv, "v:r:F:f:l:a:D:no:d:hVp:")) != -1) {
			switch (c) {
				case 'n':
					rt = 0;
					break;

				case 'f':
					format = optarg;
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

				case 'F':
					frequency = strtof(optarg, &endptr);
					goto check;

				case 'a':
					amplitude = strtof(optarg, &endptr);
					goto check;

				case 'D':
					stddev = strtof(optarg, &endptr);
					goto check;

				case 'p':
					phase = strtof(optarg, &endptr);
					goto check;

				case 'w':
					pulse_width = strtof(optarg, &endptr);
					goto check;

				case 'L':
					pulse_low = strtof(optarg, &endptr);
					goto check;

				case 'H':
					pulse_high = strtof(optarg, &endptr);
					goto check;

				case 'd':
					logging.setLevel(optarg);
					break;

				case 'V':
					printVersion();
					exit(EXIT_SUCCESS);

				case 'h':
				case '?':
					usage();
					exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
			}

			continue;

check:			if (optarg == endptr)
				logger->warn("Failed to parse parse option argument '-{} {}'", c, optarg);
		}

		if (argc != optind + 1)
			return nullptr;

		type = argv[optind];

		json_t *json_signals = json_array();

		for (int i = 0; i < values; i++) {
			json_t *json_signal = json_pack("{ s: s, s: f, s: f, s: f, s: f, s: f, s: f, s: f, s: f }",
				"signal", strdup(type.c_str()),
				"frequency", frequency,
				"amplitude", amplitude,
				"stddev", stddev,
				"offset", offset,
				"pulse_width", pulse_width,
				"pulse_low", pulse_low,
				"pulse_high", pulse_high,
				"phase", phase
			);

			json_array_append_new(json_signals, json_signal);
		}

		return json_pack("{ s: s, s: f, s: b, s: i, s: { s: o } }",
			"type", "signal",
			"rate", rate,
			"realtime", rt,
			"limit", limit,
			"in",
				"signals", json_signals
		);
	}

	void handler(int signal, siginfo_t *sinfo, void *ctx)
	{
		switch (signal)  {
			case  SIGALRM:
				logger->info("Reached timeout. Terminating...");
				break;

			default:
				logger->info("Received {} signal. Terminating...", strsignal(signal));
		}

		stop = true;
	}

	int main()
	{
		int ret;
		json_t *json, *json_format;
		json_error_t err;

		struct Sample *t;

		node = NodeFactory::make("signal.v2");
		if (!node)
			throw MemoryAllocationError();

		json = parse_cli(argc, argv);
		if (!json) {
			usage();
			exit(EXIT_FAILURE);
		}

		uuid_t uuid;
		uuid_clear(uuid);

		ret = node->parse(json, uuid);
		if (ret) {
			usage();
			exit(EXIT_FAILURE);
		}

		ret = node->getFactory()->start(nullptr);
		if (ret)
			throw RuntimeError("Failed to intialize node type {}: reason={}", *node->getFactory(), ret);

		ret = node->check();
		if (ret)
			throw RuntimeError("Failed to verify node configuration");

		ret = node->prepare();
		if (ret)
			throw RuntimeError("Failed to prepare node {}: reason={}", *node, ret);

		/* Try parsing format config as JSON */
		json_format = json_loads(format.c_str(), 0, &err);
		formatter = json_format
				? FormatFactory::make(json_format)
				: FormatFactory::make(format);
		if (!formatter)
			throw RuntimeError("Failed to initialize output");

		formatter->start(node->getInputSignals(), ~(int) SampleFlags::HAS_OFFSET);

		ret = pool_init(&pool, 16, SAMPLE_LENGTH(node->getInputSignals()->size()), &memory::heap);
		if (ret)
			throw RuntimeError("Failed to initialize pool");

		ret = node->start();
		if (ret)
			throw RuntimeError("Failed to start node {}: reason={}", *node, ret);

		while (!stop && node->getState() == State::STARTED) {
			t = sample_alloc(&pool);

retry:			ret = node->read(&t, 1);
			if (ret == 0)
				goto retry;
			else if (ret < 0)
				goto out;

			formatter->print(stdout, t);
			fflush(stdout);

out:			sample_decref(t);
		}

		ret = node->stop();
		if (ret)
			throw RuntimeError("Failed to stop node");

		ret = node->getFactory()->stop();
		if (ret)
			throw RuntimeError("Failed to de-intialize node type {}: reason={}", *node->getFactory(), ret);

		delete node;
		delete formatter;

		ret = pool_destroy(&pool);
		if (ret)
			throw RuntimeError("Failed to destroy pool");

		return 0;
	}
};

} /* namespace tools */
} /* namespace node */
} /* namespace villas */

int main(int argc, char *argv[])
{
	villas::node::tools::Signal t(argc, argv);

	return t.run();
}
