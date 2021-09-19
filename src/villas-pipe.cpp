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
 *
 * @addtogroup tools Test and debug tools
 * @{
 *********************************************************************************/

#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <signal.h>

#include <thread>
#include <iostream>
#include <atomic>

#include <villas/node/config.h>
#include <villas/config_helper.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/utils.hpp>
#include <villas/log.hpp>
#include <villas/colors.hpp>
#include <villas/node.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/format.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/exceptions.hpp>
#include <villas/nodes/websocket.hpp>
#include <villas/tool.hpp>

namespace villas {
namespace node {
namespace tools {

class PipeDirection {

protected:
	struct pool pool;
	struct vnode *node;
	Format *formatter;

	std::thread thread;
	Logger logger;

	bool stop;
	bool enabled;
	int limit;
public:
	PipeDirection(struct vnode *n, Format *fmt, bool en, int lim, const std::string &name) :
		node(n),
		formatter(fmt),
		stop(false),
		enabled(en),
		limit(lim)
	{
		auto loggerName = fmt::format("pipe:{}", name);
		logger = logging.get(loggerName);

		/* Initialize memory */
		unsigned pool_size = LOG2_CEIL(MAX(node->out.vectorize, node->in.vectorize));

		int ret = pool_init(&pool, pool_size, SAMPLE_LENGTH(DEFAULT_SAMPLE_LENGTH), node_memory_type(node));
		if (ret < 0)
			throw RuntimeError("Failed to allocate memory for pool.");
	}

	~PipeDirection()
	{
		int ret __attribute__((unused));

		ret = pool_destroy(&pool);
	}

	virtual void run()
	{

	}

	void startThread()
	{
		stop = false;
		if (enabled)
			thread = std::thread(&villas::node::tools::PipeDirection::run, this);
	}

	void stopThread()
	{
		stop = true;

		/* We send a signal to the thread in order to interrupt blocking system calls */
		pthread_kill(thread.native_handle(), SIGUSR1);

		thread.join();
	}
};

class PipeSendDirection : public PipeDirection {

public:
	PipeSendDirection(struct vnode *n, Format *i, bool en = true, int lim = -1) :
		PipeDirection(n, i, en, lim, "send")
	{ }

	virtual void run()
	{
		unsigned last_sequenceno = 0;
		int scanned, sent, allocated, cnt = 0;

		struct sample *smps[node->out.vectorize];

		while (!stop && !feof(stdin)) {
			allocated = sample_alloc_many(&pool, smps, node->out.vectorize);
			if (allocated < 0)
				throw RuntimeError("Failed to get {} samples out of send pool.", node->out.vectorize);
			else if (allocated < (int) node->out.vectorize)
				logger->warn("Send pool underrun");

			scanned = formatter->scan(stdin, smps, allocated);
			if (scanned < 0) {
				if (stop)
					goto leave2;

				logger->warn("Failed to read samples from stdin");
				continue;
			}
			else if (scanned == 0)
				continue;

			/* Fill in missing sequence numbers */
			for (int i = 0; i < scanned; i++) {
				if (smps[i]->flags & (int) SampleFlags::HAS_SEQUENCE)
					last_sequenceno = smps[i]->sequence;
				else
					smps[i]->sequence = last_sequenceno++;
			}

			sent = node_write(node, smps, scanned);

			sample_decref_many(smps, scanned);

			cnt += sent;
			if (limit > 0 && cnt >= limit)
				goto leave;
		}

leave2:
		logger->info("Send thread stopped");
		return;

leave:		if (feof(stdin)) {
			if (limit < 0) {
				logger->info("Reached end-of-file. Terminating...");
				raise(SIGINT);
			}
			else
				logger->info("Reached end-of-file. Wait for receive side...");
		}
		else {
			logger->info("Reached send limit. Terminating...");
			raise(SIGINT);
		}
	}
};

class PipeReceiveDirection : public PipeDirection {

public:
	PipeReceiveDirection(struct vnode *n, Format *i, bool en = true, int lim = -1) :
		PipeDirection(n, i, en, lim, "recv")
	{ }

	virtual void run()
	{
		int recv, cnt = 0, allocated = 0;
		struct sample *smps[node->in.vectorize];

		while (!stop) {
			allocated = sample_alloc_many(&pool, smps, node->in.vectorize);
			if (allocated < 0)
				throw RuntimeError("Failed to allocate {} samples from receive pool.", node->in.vectorize);
			else if (allocated < (int) node->in.vectorize)
				logger->warn("Receive pool underrun: allocated only {} of {} samples", allocated, node->in.vectorize);

			recv = node_read(node, smps, allocated);
			if (recv < 0) {
				if (node->state == State::STOPPING || stop)
					goto leave2;
				else
					logger->warn("Failed to receive samples from node {}: reason={}", *node, recv);
			}
			else {
				formatter->print(stdout, smps, recv);

				cnt += recv;
				if (limit > 0 && cnt >= limit)
					goto leave;
			}

			sample_decref_many(smps, allocated);
		}

		return;

leave:
		logger->info("Reached receive limit. Terminating...");
leave2:
		logger->info("Receive thread stopped");
		raise(SIGINT);
	}
};

class Pipe : public Tool {

public:
	Pipe(int argc, char *argv[]) :
		Tool(argc, argv, "pipe"),
		stop(false),
		formatter(),
		timeout(0),
		reverse(false),
		format("villas.human"),
		dtypes("64f"),
		config_cli(json_object()),
		enable_send(true),
		enable_recv(true),
		limit_send(-1),
		limit_recv(-1)
	{
		int ret;

		ret = memory_init(DEFAULT_NR_HUGEPAGES);
		if (ret)
			throw RuntimeError("Failed to initialize memory");
	}

	~Pipe()
	{
		json_decref(config_cli);
	}

protected:
	std::atomic<bool> stop;

	SuperNode sn; /**< The global configuration */
	Format *formatter;

	int timeout;
	bool reverse;
	std::string format;
	std::string dtypes;
	std::string uri;
	std::string nodestr;

	json_t *config_cli;

	bool enable_send;
	bool enable_recv;
	int limit_send;
	int limit_recv;

	void handler(int signal, siginfo_t *sinfo, void *ctx)
	{
		switch (signal)  {
			case SIGALRM:
				logger->info("Reached timeout. Terminating...");
				break;

			case SIGUSR1:
				break; /* ignore silently */

			default:
				logger->info("Received {} signal. Terminating...", strsignal(signal));
				break;
		}

		stop = true;
	}

	void usage()
	{
		std::cout << "Usage: villas-pipe [OPTIONS] CONFIG NODE" << std::endl
			<< "  CONFIG  path to a configuration file" << std::endl
			<< "  NODE    the name of the node to which samples are sent and received from" << std::endl
			<< "  OPTIONS are:" << std::endl
			<< "    -f FMT           set the format" << std::endl
			<< "    -t DT            the data-type format string" << std::endl
			<< "    -o OPTION=VALUE  overwrite options in config file" << std::endl
			<< "    -x               swap read / write endpoints" << std::endl
			<< "    -s               only read data from stdin and send it to node" << std::endl
			<< "    -r               only read data from node and write it to stdout" << std::endl
			<< "    -T NUM           terminate after NUM seconds" << std::endl
			<< "    -L NUM           terminate after NUM samples sent" << std::endl
			<< "    -l NUM           terminate after NUM samples received" << std::endl
			<< "    -h               show this usage information" << std::endl
			<< "    -d               set logging level" << std::endl
			<< "    -V               show the version of the tool" << std::endl << std::endl;

		printCopyright();
	}

	void parse()
	{
		int c, ret;
		char *endptr;
		while ((c = getopt(argc, argv, "Vhxrsd:l:L:T:f:t:o:")) != -1) {
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

				case 'x':
					reverse = true;
					break;

				case 's':
					enable_recv = false; // send only
					break;

				case 'r':
					enable_send = false; // receive only
					break;

				case 'l':
					limit_recv = strtoul(optarg, &endptr, 10);
					goto check;

				case 'L':
					limit_send = strtoul(optarg, &endptr, 10);
					goto check;

				case 'T':
					timeout = strtoul(optarg, &endptr, 10);
					goto check;

				case 'o':
					ret = json_object_extend_str(config_cli, optarg);
					if (ret)
						throw RuntimeError("Invalid option: {}", optarg);
					break;

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
		nodestr = argv[optind+1];
	}

	int main()
	{
		int ret;
		struct vnode *node;
		json_t *json_format;
		json_error_t err;

		logger->info("Logging level: {}", logging.getLevelName());

		if (!uri.empty())
			sn.parse(uri);
		else
			logger->warn("No configuration file specified. Starting unconfigured. Use the API to configure this instance.");


		/* Try parsing format config as JSON */
		json_format = json_loads(format.c_str(), 0, &err);
		formatter = json_format
				? FormatFactory::make(json_format)
				: FormatFactory::make(format);
		if (!formatter)
			throw RuntimeError("Failed to initialize formatter");

		formatter->start(dtypes);

		node = sn.getNode(nodestr);
		if (!node)
			throw RuntimeError("Node {} does not exist!", nodestr);

		if (!node_type(node)->read && enable_recv)
			throw RuntimeError("Node {} can not receive data. Consider using send-only mode by using '-s' option", nodestr);

		if (!node_type(node)->write && enable_send)
			throw RuntimeError("Node {} can not send data. Consider using receive-only mode by using '-r' option", nodestr);

#if defined(WITH_NODE_WEBSOCKET) && defined(WITH_WEB)
		/* Only start web subsystem if villas-pipe is used with a websocket node */
		if (node_type(node)->start == websocket_start) {
			Web *w = sn.getWeb();
			w->start();
		}
#endif /* WITH_NODE_WEBSOCKET */

		if (reverse)
			node_reverse(node);

		ret = node_type_start(node_type(node), &sn);
		if (ret)
			throw RuntimeError("Failed to intialize node type {}: reason={}", *node_type(node), ret);

		sn.startInterfaces();

		ret = node_check(node);
		if (ret)
			throw RuntimeError("Invalid node configuration");

		ret = node_prepare(node);
		if (ret)
			throw RuntimeError("Failed to prepare node {}: reason={}", *node, ret);

		ret = node_start(node);
		if (ret)
			throw RuntimeError("Failed to start node {}: reason={}", *node, ret);

		PipeReceiveDirection recv_dir(node, formatter, enable_recv, limit_recv);
		PipeSendDirection send_dir(node, formatter, enable_send, limit_send);

		recv_dir.startThread();
		send_dir.startThread();

		alarm(timeout);

		while (!stop)
			sleep(1);

		recv_dir.stopThread();
		send_dir.stopThread();

		ret = node_stop(node);
		if (ret)
			throw RuntimeError("Failed to stop node {}: reason={}", *node, ret);

		sn.stopInterfaces();

		ret = node_type_stop(node_type(node));
		if (ret)
			throw RuntimeError("Failed to stop node type {}: reason={}", *node_type(node), ret);

#if defined(WITH_NODE_WEBSOCKET) && defined(WITH_WEB)
		/* Only start web subsystem if villas-pipe is used with a websocket node */
		if (node_type(node)->stop == websocket_stop) {
			Web *w = sn.getWeb();
			w->stop();
		}
#endif /* WITH_NODE_WEBSOCKET */

		delete formatter;

		return 0;
	}
};


} /* namespace tools */
} /* namespace node */
} /* namespace villas */

int main(int argc, char *argv[])
{
	villas::node::tools::Pipe t(argc, argv);

	return t.run();
}

/** @} */
