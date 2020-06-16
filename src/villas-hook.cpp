/** Receive messages from server snd print them on stdout.
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
 *********************************************************************************/

/**
 * @addtogroup tools Test and debug tools
 * @{
 */

#include <iostream>
#include <atomic>
#include <unistd.h>

#include <villas/tool.hpp>
#include <villas/timing.h>
#include <villas/sample.h>
#include <villas/io.h>
#include <villas/hook.hpp>
#include <villas/utils.hpp>
#include <villas/pool.h>
#include <villas/log.hpp>
#include <villas/colors.hpp>
#include <villas/exceptions.hpp>
#include <villas/plugin.hpp>
#include <villas/plugin.h>
#include <villas/config_helper.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/node/config.h>

using namespace villas;
using namespace villas::node;
using namespace villas::plugin;

namespace villas {
namespace node {
namespace tools {

class Hook : public Tool {

public:
	Hook(int argc, char *argv[]) :
		Tool(argc, argv, "hook"),
		stop(false),
		format("villas.human"),
		dtypes("64f"),
		cnt(1)
	{
		int ret;

		ret = memory_init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");

		cfg_cli = json_object();
	}

	~Hook()
	{
		json_decref(cfg_cli);
	}

protected:

	std::atomic<bool> stop;

	std::string hook;

	std::string format;
	std::string dtypes;

	struct pool p;
	struct io  io;

	int cnt;

	json_t *cfg_cli;

	void handler(int signal, siginfo_t *sinfo, void *ctx)
	{
		stop = true;
	}

	void usage()
	{
		std::cout << "Usage: villas-hook [OPTIONS] NAME [[PARAM1] [PARAM2] ...]" << std::endl
			<< "  NAME      the name of the hook function" << std::endl
			<< "  PARAM*    a string of configuration settings for the hook" << std::endl
			<< "  OPTIONS is one or more of the following options:" << std::endl
			<< "    -f FMT  the data format" << std::endl
			<< "    -t DT   the data-type format string" << std::endl
			<< "    -d LVL  set debug level to LVL" << std::endl
			<< "    -v CNT  process CNT smps at once" << std::endl
			<< "    -h      show this help" << std::endl
			<< "    -V      show the version of the tool" << std::endl << std::endl;

#ifdef WITH_HOOKS
		std::cout << "Supported hooks:" << std::endl;
		for (Plugin *p : Registry::lookup<HookFactory>())
			std::cout << " - " << p->getName() << ": " << p->getDescription() << std::endl;
		std::cout << std::endl;
#endif /* WITH_HOOKS */

		std::cout << "Supported IO formats:" << std::endl;
		plugin_dump(PluginType::FORMAT);
		std::cout << std::endl;

		std::cout << "Example:" << std::endl
			<< "  villas-signal random | villas-hook skip_first seconds=10" << std::endl
			<< std::endl;

		printCopyright();
	}

	void parse()
	{
		int ret;

		/* Parse optional command line arguments */
		int c;
		char *endptr;
		while ((c = getopt(argc, argv, "Vhv:d:f:t:o:")) != -1) {
			switch (c) {
				case 'V':
					printVersion();
					exit(EXIT_SUCCESS);

				case 'f':
					format = optarg;
					break;

				case 't':
					dtypes = optarg;
					break;

				case 'v':
					cnt = strtoul(optarg, &endptr, 0);
					goto check;

				case 'd':
					logging.setLevel(optarg);
					break;

				case 'o':
					ret = json_object_extend_str(cfg_cli, optarg);
					if (ret)
						throw RuntimeError("Invalid option: {}", optarg);
					break;

				case '?':
				case 'h':
					usage();
					exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
			}

			continue;

check:			if (optarg == endptr)
				throw RuntimeError("Failed to parse parse option argument '-{} {}'", c, optarg);

		}

		if (argc < optind + 1) {
			usage();
			exit(EXIT_FAILURE);
		}

		hook = argv[optind];
	}

	int main()
	{
		int ret, recv, sent;

		struct format_type *ft;
		struct sample **smps;

		if (cnt < 1)
			throw RuntimeError("Vectorize option must be greater than 0");

		smps = new struct sample*[cnt];

		ret = pool_init(&p, 10 * cnt, SAMPLE_LENGTH(DEFAULT_SAMPLE_LENGTH));
		if (ret)
			throw RuntimeError("Failed to initilize memory pool");

		/* Initialize IO */
		ft = format_type_lookup(format.c_str());
		if (!ft)
			throw RuntimeError("Unknown IO format '{}'", format);

		ret = io_init2(&io, ft, dtypes.c_str(), (int) SampleFlags::HAS_ALL);
		if (ret)
			throw RuntimeError("Failed to initialize IO");

		ret = io_check(&io);
		if (ret)
			throw RuntimeError("Failed to validate IO configuration");

		ret = io_open(&io, nullptr);
		if (ret)
			throw RuntimeError("Failed to open IO");

		/* Initialize hook */
		auto hf = plugin::Registry::lookup<HookFactory>(hook);
		if (!hf)
			throw RuntimeError("Unknown hook function '{}'", hook);

		auto h = hf->make(nullptr, nullptr);
		if (!h)
			throw RuntimeError("Failed to initialize hook");

		h->parse(cfg_cli);
		h->check();
		h->prepare(io.signals);
		h->start();

		while (!stop) {
			ret = sample_alloc_many(&p, smps, cnt);
			if (ret != cnt)
				throw RuntimeError("Failed to allocate %d smps from pool", cnt);

			recv = io_scan(&io, smps, cnt);
			if (recv < 0) {
				if (io_eof(&io))
					break;

				throw RuntimeError("Failed to read from stdin");
			}

			timespec now = time_now();

			logger->debug("Read {} smps from stdin", recv);

			unsigned send = 0;
			for (int processed = 0; processed < recv; processed++) {
				struct sample *smp = smps[processed];

				if (!(smp->flags & (int) SampleFlags::HAS_TS_RECEIVED)){
					smp->ts.received = now;
					smp->flags |= (int) SampleFlags::HAS_TS_RECEIVED;
				}

				auto ret = h->process(smp);
				switch (ret) {
					using Reason = villas::node::Hook::Reason;

					case Reason::ERROR:
						throw RuntimeError("Failed to process samples");

					case Reason::OK:
						smps[send++] = smp;
						break;

					case Reason::SKIP_SAMPLE:
						break;

					case Reason::STOP_PROCESSING:
						goto stop;
				}
			}

stop:			sent = io_print(&io, smps, send);
			if (sent < 0)
				throw RuntimeError("Failed to write to stdout");

			sample_free_many(smps, cnt);
		}

		h->stop();

		delete h;

		ret = io_close(&io);
		if (ret)
			throw RuntimeError("Failed to close IO");

		ret = io_destroy(&io);
		if (ret)
			throw RuntimeError("Failed to destroy IO");

		sample_free_many(smps, cnt);

		delete smps;

		ret = pool_destroy(&p);
		if (ret)
			throw RuntimeError("Failed to destroy memory pool");

		return 0;
	}
};

} /* namespace tools */
} /* namespace node */
} /* namespace villas */

int main(int argc, char *argv[])
{
	villas::node::tools::Hook t(argc, argv);

	return t.run();
}
