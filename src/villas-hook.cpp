/** Receive messages from server snd print them on stdout.
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
 *********************************************************************************/

#include <iostream>
#include <atomic>
#include <unistd.h>

#include <villas/tool.hpp>
#include <villas/timing.hpp>
#include <villas/sample.hpp>
#include <villas/format.hpp>
#include <villas/hook.hpp>
#include <villas/utils.hpp>
#include <villas/pool.hpp>
#include <villas/log.hpp>
#include <villas/colors.hpp>
#include <villas/exceptions.hpp>
#include <villas/plugin.hpp>
#include <villas/config_helper.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/node/config.hpp>
#include <villas/node/exceptions.hpp>

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
		input_format("villas.human"),
		dtypes("64f"),
		p(),
		input(),
		output(),
		cnt(1)
	{
		int ret;

		ret = memory::init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");

		config = json_object();
	}

	~Hook()
	{
		json_decref(config);
	}

protected:

	std::atomic<bool> stop;

	std::string hook;

	std::string output_format;
	std::string input_format;
	std::string dtypes;

	struct Pool p;
	Format *input;
	Format *output;

	int cnt;

	json_t *config;

	void handler(int signal, siginfo_t *sinfo, void *ctx)
	{
		stop = true;
	}

	void usage()
	{
		std::cout << "Usage: villas-hook [OPTIONS] NAME" << std::endl
			<< "  NAME      the name of the hook function" << std::endl
			<< "  PARAM*    a string of configuration settings for the hook" << std::endl
			<< "  OPTIONS is one or more of the following options:" << std::endl
			<< "    -c CONFIG       a JSON file containing just the hook configuration" << std::endl
			<< "    -f FMT          the input data format" << std::endl
			<< "    -F FMT          the output data format (defaults to input format)" << std::endl
			<< "    -t DT           the data-type format string" << std::endl
			<< "    -d LVL          set debug level to LVL" << std::endl
			<< "    -v CNT          process CNT smps at once" << std::endl
			<< "    -o PARAM=VALUE  provide parameters for hook configuration" << std::endl
			<< "    -h              show this help" << std::endl
			<< "    -V              show the version of the tool" << std::endl << std::endl;

		std::cout << "Supported hooks:" << std::endl;
		for (Plugin *p : Registry::lookup<HookFactory>())
			std::cout << " - " << *p << ": " << p->getDescription() << std::endl;
		std::cout << std::endl;

		std::cout << "Supported IO formats:" << std::endl;
		for (Plugin *p : Registry::lookup<FormatFactory>())
			std::cout << " - " << *p << ": " << p->getDescription() << std::endl;
		std::cout << std::endl;

		std::cout << "Example:" << std::endl
			<< "  villas-signal random | villas-hook skip_first seconds=10" << std::endl
			<< std::endl;

		printCopyright();
	}

	void parse()
	{
		int ret;
		std::string file;

		/* Parse optional command line arguments */
		int c;
		char *endptr;
		while ((c = getopt(argc, argv, "Vhv:d:f:F:t:o:c:")) != -1) {
			switch (c) {
				case 'c':
					file = optarg;
					break;

				case 'V':
					printVersion();
					exit(EXIT_SUCCESS);

				case 'f':
					input_format = optarg;
					break;

				case 'F':
					output_format = optarg;
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
					ret = json_object_extend_str(config, optarg);
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

		if (output_format.empty())
			output_format = input_format;

		if (!file.empty()) {
			json_error_t err;
			json_t *j = json_load_file(file.c_str(), 0, &err);
			if (!j)
				throw JanssonParseError(err);

			json_object_update_missing(config, j);
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
		struct Sample *smps[cnt];

		if (cnt < 1)
			throw RuntimeError("Vectorize option must be greater than 0");

		ret = pool_init(&p, 10 * cnt, SAMPLE_LENGTH(DEFAULT_SAMPLE_LENGTH));
		if (ret)
			throw RuntimeError("Failed to initilize memory pool");

		/* Initialize IO */
		struct desc {
			std::string dir;
			std::string format;
			Format **formatter;
		};
		std::list<struct desc> descs = {
			{ "in",  input_format.c_str(),  &input  },
			{ "out", output_format.c_str(), &output }
		};

		for (auto &d : descs) {
			json_t *json_format;
			json_error_t err;

			/* Try parsing format config as JSON */
			json_format = json_loads(d.format.c_str(), 0, &err);
			(*d.formatter) = json_format
				? FormatFactory::make(json_format)
				: FormatFactory::make(d.format);
			if (!(*d.formatter))
				throw RuntimeError("Failed to initialize {} IO", d.dir);

			(*d.formatter)->start(dtypes, (int) SampleFlags::HAS_ALL);
		}

		/* Initialize hook */
		auto hf = plugin::Registry::lookup<HookFactory>(hook);
		if (!hf)
			throw RuntimeError("Unknown hook function '{}'", hook);

		auto h = hf->make(nullptr, nullptr);
		if (!h)
			throw RuntimeError("Failed to initialize hook");

		h->parse(config);
		h->check();
		h->prepare(input->getSignals());
		h->start();

		while (!stop && !feof(stdin)) {
			ret = sample_alloc_many(&p, smps, cnt);
			if (ret != cnt)
				throw RuntimeError("Failed to allocate {} smps from pool", cnt);

			recv = input->scan(stdin, smps, cnt);
			if (recv < 0)
				throw RuntimeError("Failed to read from stdin");

			timespec now = time_now();

			logger->debug("Read {} smps from stdin", recv);

			unsigned send = 0;
			for (int processed = 0; processed < recv; processed++) {
				struct Sample *smp = smps[processed];

				if (!(smp->flags & (int) SampleFlags::HAS_TS_RECEIVED)){
					smp->ts.received = now;
					smp->flags |= (int) SampleFlags::HAS_TS_RECEIVED;
				}

				auto ret = h->process(smp);
				switch (ret) {
					using Reason = node::Hook::Reason;
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

				smp->signals = h->getSignals();
			}

stop:			sent = output->print(stdout, smps, send);
			if (sent < 0)
				throw RuntimeError("Failed to write to stdout");

			sample_free_many(smps, cnt);
		}

		h->stop();

		for (auto &d : descs)
			delete (*d.formatter);

		sample_free_many(smps, cnt);

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
