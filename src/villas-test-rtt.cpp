/** Measure round-trip time.
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

#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <cctype>
#include <sys/stat.h>
#include <cinttypes>
#include <iostream>
#include <atomic>

#include <villas/tool.hpp>
#include <villas/node/config.h>
#include <villas/super_node.hpp>
#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/node.h>
#include <villas/utils.hpp>
#include <villas/hist.hpp>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/kernel/rt.hpp>

#define CLOCK_ID	CLOCK_MONOTONIC

using namespace villas;
using namespace villas::node;

namespace villas {
namespace node {
namespace tools {

class TestRtt : public Tool {

public:
	TestRtt(int argc, char *argv[]) :
		Tool(argc, argv, "test-rtt"),
		stop(false),
		fd(STDOUT_FILENO),
		count(-1),
		hist_warmup(100),
		hist_buckets(20)
	{
		int ret;

		ret = memory_init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");
	}

protected:
	std::atomic<bool> stop;

	std::string uri;
	std::string nodestr;

	SuperNode sn;

	/** File descriptor for Matlab results.
	 * This allows you to write Matlab results in a seperate log file:
	 *
	 *    ./test etc/example.conf rtt -f 3 3>> measurement_results.m
	 */
	int fd;

	/**< Amount of messages which should be sent (default: -1 for unlimited) */
	int count;


	Hist::cnt_t hist_warmup;
	int hist_buckets;

	void handler(int signal, siginfo_t *sinfo, void *ctx)
	{
		stop = true;
	}

	void usage()
	{
		std::cout << "Usage: villas-test-rtt [OPTIONS] CONFIG NODE" << std::endl
			<< "  CONFIG  path to a configuration file" << std::endl
			<< "  NODE    name of the node which shoud be used" << std::endl
			<< "  OPTIONS is one or more of the following options:" << std::endl
			<< "    -c CNT  send CNT messages" << std::endl
			<< "    -f FD   use file descriptor FD for result output instead of stdout" << std::endl
			<< "    -b BKTS number of buckets for histogram" << std::endl
			<< "    -w WMUP duration of histogram warmup phase" << std::endl
			<< "    -h      show this usage information" << std::endl
			<< "    -V      show the version of the tool" << std::endl << std::endl;

		printCopyright();
	}

	void parse()
	{
		/* Parse Arguments */
		int c;
		char *endptr;
		while ((c = getopt (argc, argv, "w:hr:f:c:b:Vd:")) != -1) {
			switch (c) {
				case 'c':
					count = strtoul(optarg, &endptr, 10);
					goto check;

				case 'f':
					fd = strtoul(optarg, &endptr, 10);
					goto check;

				case 'w':
					hist_warmup = strtoul(optarg, &endptr, 10);
					goto check;

				case 'b':
					hist_buckets = strtoul(optarg, &endptr, 10);
					goto check;

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

		if (argc != optind + 2) {
			usage();
			exit(EXIT_FAILURE);
		}

		uri = argv[optind];
		nodestr = argv[optind + 1];
	}

	int main()
	{
		int ret;

		Hist hist(hist_buckets, hist_warmup);
		struct timespec send, recv;

		struct sample *smp_send = (struct sample *) new char[SAMPLE_LENGTH(2)];
		struct sample *smp_recv = (struct sample *) new char[SAMPLE_LENGTH(2)];

		if (!smp_send || !smp_recv)
			throw MemoryAllocationError();

		struct vnode *node;

		if (!uri.empty())
			sn.parse(uri);
		else
			logger->warn("No configuration file specified. Starting unconfigured. Use the API to configure this instance.");

		node = sn.getNode(nodestr);
		if (!node)
			throw RuntimeError("There's no node with the name '{}'", nodestr);

		ret = node_type_start(node->_vt, &sn);
		if (ret)
			throw RuntimeError("Failed to start node-type {}: reason={}", node_type_name(node->_vt), ret);

		ret = node_prepare(node);
		if (ret)
			throw RuntimeError("Failed to start node {}: reason={}", node_name(node), ret);

		ret = node_start(node);
		if (ret)
			throw RuntimeError("Failed to start node {}: reason={}", node_name(node), ret);

		/* Print header */
		fprintf(stdout, "%17s%5s%10s%10s%10s%10s%10s\n", "timestamp", "seq", "rtt", "min", "max", "mean", "stddev");

		while (!stop && (count < 0 || count--)) {
			clock_gettime(CLOCK_ID, &send);

			unsigned release;

			release = 1; // release = allocated
			node_write(node, &smp_send, 1, &release); /* Ping */

			release = 1; // release = allocated
			node_read(node,  &smp_recv, 1, &release); /* Pong */

			clock_gettime(CLOCK_ID, &recv);

			double rtt = time_delta(&recv, &send);

			if (rtt < 0)
				logger->warn("Negative RTT: {}", rtt);

			hist.put(rtt);

			smp_send->sequence++;

			fprintf(stdout, "%10lld.%06lld%5" PRIu64 "%10.3f%10.3f%10.3f%10.3f%10.3f\n",
				(long long) recv.tv_sec,
				(long long) recv.tv_nsec / 1000, smp_send->sequence,
				1e3 * rtt, 1e3 * hist.getLowest(), 1e3 * hist.getHighest(),
				1e3 * hist.getMean(), 1e3 * hist.getStddev());
		}

		struct stat st;
		if (!fstat(fd, &st)) {
			FILE *f = fdopen(fd, "w");
			hist.dumpMatlab(f);
			fclose(f);
		}
		else
			throw RuntimeError("Invalid file descriptor: {}", fd);

		hist.print(true);

		ret = node_stop(node);
		if (ret)
			throw RuntimeError("Failed to stop node {}: reason={}", node_name(node), ret);

		ret = node_type_stop(node->_vt);
		if (ret)
			throw RuntimeError("Failed to stop node-type {}: reason={}", node_type_name(node->_vt), ret);

		delete smp_send;
		delete smp_recv;

		return 0;
	}
};

} /* namespace tools */
} /* namespace node */
} /* namespace villas */

int main(int argc, char *argv[])
{
	villas::node::tools::TestRtt t(argc, argv);

	return t.run();
}
