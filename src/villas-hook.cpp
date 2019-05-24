/** Receive messages from server snd print them on stdout.
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
 *********************************************************************************/

/**
 * @addtogroup tools Test and debug tools
 * @{
 */

#include <iostream>
#include <atomic>
#include <unistd.h>

#include <villas/timing.h>
#include <villas/sample.h>
#include <villas/io.h>
#include <villas/hook.hpp>
#include <villas/utils.hpp>
#include <villas/pool.h>
#include <villas/log.hpp>
#include <villas/colors.hpp>
#include <villas/exceptions.hpp>
#include <villas/copyright.hpp>
#include <villas/plugin.hpp>
#include <villas/plugin.h>
#include <villas/config_helper.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/node/config.h>

using namespace villas;
using namespace villas::node;
using namespace villas::plugin;

static std::atomic<bool> stop(false);

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	stop = true;
}

static void usage()
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
	plugin_dump(PLUGIN_TYPE_FORMAT);
	std::cout << std::endl;

	std::cout << "Example:" << std::endl
	          << "  villas-signal random | villas-hook skip_first seconds=10" << std::endl
	          << std::endl;

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret, recv, sent, cnt;
	const char *format = "villas.human";
	const char *dtypes = "64f";

	struct format_type *ft;
	struct sample **smps;

	Logger logger = logging.get("hook");

	try {
		struct pool p;
		struct io  io;

		p.state = STATE_DESTROYED;
		io.state = STATE_DESTROYED;

		/* Default values */
		cnt = 1;

		json_t *cfg_cli = json_object();

		/* Parse optional command line arguments */
		int c;
		char *endptr;
		while ((c = getopt(argc, argv, "Vhv:d:f:t:o:")) != -1) {
			switch (c) {
				case 'V':
					print_version();
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

		char *hook = argv[optind];

		ret = utils::signals_init(quit);
		if (ret)
			throw RuntimeError("Failed to intialize signals");

		if (cnt < 1)
			throw RuntimeError("Vectorize option must be greater than 0");

		ret = memory_init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");

		smps = new struct sample*[cnt];

		ret = pool_init(&p, 10 * cnt, SAMPLE_LENGTH(DEFAULT_SAMPLE_LENGTH), &memory_hugepage);
		if (ret)
			throw RuntimeError("Failed to initilize memory pool");

		/* Initialize IO */
		ft = format_type_lookup(format);
		if (!ft)
			throw RuntimeError("Unknown IO format '{}'", format);

		ret = io_init2(&io, ft, dtypes, SAMPLE_HAS_ALL);
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

				if (!(smp->flags & SAMPLE_HAS_TS_RECEIVED)){
					smp->ts.received = now;
					smp->flags |= SAMPLE_HAS_TS_RECEIVED;
				}

				ret = h->process(smp);
				switch (ret) {
					case HOOK_ERROR:
						throw RuntimeError("Failed to process samples");

					case HOOK_OK:
						smps[send++] = smp;
						break;

					case HOOK_SKIP_SAMPLE:
						break;

					case HOOK_STOP_PROCESSING:
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

		logger->info(CLR_GRN("Goodbye!"));

		return 0;
	}
	catch (std::runtime_error &e) {
		logger->error("{}", e.what());

		return -1;
	}
}
