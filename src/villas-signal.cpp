/** Generate random packages on stdout.
 *
 * @file
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
 *
 * @addtogroup tools Test and debug tools
 * @{
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
#include <villas/sample.h>
#include <villas/timing.h>
#include <villas/node.h>
#include <villas/pool.h>
#include <villas/plugin.h>
#include <villas/nodes/signal_generator.hpp>

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

		ret = memory_init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");
	}

protected:
	std::atomic<bool> stop;

	struct vnode node;
	Format *formatter;
	struct pool pool;

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
		std::string type;
		int rt = 1;
		int values = 1;
		int limit = -1;

		/* Parse optional command line arguments */
		int c;
		char *endptr;
		while ((c = getopt(argc, argv, "v:r:F:f:l:a:D:no:d:hV")) != -1) {
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

		return json_pack("{ s: s, s: s, s: f, s: f, s: f, s: f, s: f, s: b, s: i, s: i }",
			"type", "signal",
			"signal", type.c_str(),
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
		struct vnode_type *nt;

		struct sample *t;

		nt = node_type_lookup("signal");
		if (!nt)
			throw RuntimeError("Signal generation is not supported.");

		ret = node_init(&node, nt);
		if (ret)
			throw RuntimeError("Failed to initialize node");

		json = parse_cli(argc, argv);
		if (!json) {
			usage();
			exit(EXIT_FAILURE);
		}

		uuid_t uuid;
		uuid_clear(uuid);

		ret = node_parse(&node, json, uuid);
		if (ret) {
			usage();
			exit(EXIT_FAILURE);
		}

		// nt == n._vt
		ret = node_type_start(nt, nullptr);
		if (ret)
			throw RuntimeError("Failed to initialize node type: {}", node_type_name(nt));

		ret = node_check(&node);
		if (ret)
			throw RuntimeError("Failed to verify node configuration");

		ret = node_prepare(&node);
		if (ret)
			throw RuntimeError("Failed to prepare node {}: reason={}", node_name(&node), ret);

		/* Try parsing format config as JSON */
		json_format = json_loads(format.c_str(), 0, &err);
		formatter = json_format
				? FormatFactory::make(json_format)
				: FormatFactory::make(format);
		if (!formatter)
			throw RuntimeError("Failed to initialize output");

		formatter->start(&node.in.signals, ~(int) SampleFlags::HAS_OFFSET);

		ret = pool_init(&pool, 16, SAMPLE_LENGTH(vlist_length(&node.in.signals)), &memory_heap);
		if (ret)
			throw RuntimeError("Failed to initialize pool");

		ret = node_start(&node);
		if (ret)
			throw RuntimeError("Failed to start node {}: reason={}", node_name(&node), ret);

		while (!stop && node.state == State::STARTED) {
			t = sample_alloc(&pool);

retry:			ret = node_read(&node, &t, 1);
			if (ret == 0)
				goto retry;
			else if (ret < 0)
				goto out;

			formatter->print(stdout, t);
			fflush(stdout);

out:			sample_decref(t);
		}

		ret = node_stop(&node);
		if (ret)
			throw RuntimeError("Failed to stop node");

		ret = node_destroy(&node);
		if (ret)
			throw RuntimeError("Failed to destroy node");

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

/** @} */
