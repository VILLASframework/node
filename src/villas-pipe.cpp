/** Receive messages from server snd print them on stdout.
 *
 * @file
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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
#include <unistd.h>
#include <signal.h>

#include <thread>
#include <iostream>
#include <atomic>

#include <villas/node/config.hpp>
#include <villas/config_helper.hpp>
#include <villas/super_node.hpp>
#include <villas/utils.hpp>
#include <villas/utils.hpp>
#include <villas/log.hpp>
#include <villas/colors.hpp>
#include <villas/node.hpp>
#include <villas/timing.hpp>
#include <villas/pool.hpp>
#include <villas/format.hpp>
#include <villas/kernel/rt.hpp>
#include <villas/exceptions.hpp>
#include <villas/nodes/websocket.hpp>
#include <villas/tool.hpp>

namespace villas {
namespace node {
namespace tools {

class Pipe;

class PipeDirection {

protected:
	struct Pool pool;
	Node *node;
	Format *formatter;

	std::thread thread;
	Logger logger;

	bool stop;
	bool enabled;
	int limit;
	int count;
public:
	PipeDirection(Node *n, Format *fmt, bool en, int lim, const std::string &name) :
		node(n),
		formatter(fmt),
		stop(false),
		enabled(en),
		limit(lim),
		count(0)
	{
		auto loggerName = fmt::format("pipe:{}", name);
		logger = logging.get(loggerName);

		/* Initialize memory */
		unsigned pool_size = LOG2_CEIL(MAX(node->out.vectorize, node->in.vectorize));

		int ret = pool_init(&pool, pool_size, SAMPLE_LENGTH(DEFAULT_SAMPLE_LENGTH), node->getMemoryType());
		if (ret < 0)
			throw RuntimeError("Failed to allocate memory for pool.");
	}

	~PipeDirection()
	{
		int ret __attribute__((unused));

		ret = pool_destroy(&pool);
	}

	virtual
	void run() = 0;

	void startThread()
	{
		stop = false;

		if (enabled)
			thread = std::thread(&PipeDirection::run, this);
	}

	void stopThread()
	{
		stop = true;

		if (enabled) {
			// We send a SIGUSR2 to the threads to unblock their blocking read() syscalls
			pthread_kill(thread.native_handle(), SIGUSR2);
			thread.join();
		}
	}
};

class PipeSendDirection : public PipeDirection {

	friend Pipe;

public:
	PipeSendDirection(Node *n, Format *i, bool en = true, int lim = -1) :
		PipeDirection(n, i, en, lim, "send")
	{ }

	virtual
	void run()
	{
		logger->debug("Send thread started");

		unsigned last_sequenceno = 0;
		int scanned, sent, allocated;

		struct Sample *smps[node->out.vectorize];

		while (!stop) {
			allocated = sample_alloc_many(&pool, smps, node->out.vectorize);
			if (allocated < 0)
				throw RuntimeError("Failed to get {} samples out of send pool.", node->out.vectorize);
			else if (allocated < (int) node->out.vectorize)
				logger->warn("Send pool underrun");

			scanned = formatter->scan(stdin, smps, allocated);
			if (scanned < 0) {
				if (!stop)
					logger->warn("Failed to read from stdin");
			}

			/* Fill in missing sequence numbers */
			for (int i = 0; i < scanned; i++) {
				if (smps[i]->flags & (int) SampleFlags::HAS_SEQUENCE)
					last_sequenceno = smps[i]->sequence;
				else
					smps[i]->sequence = last_sequenceno++;
			}

			sent = node->write(smps, scanned);

			sample_decref_many(smps, allocated);

			count += sent;
			if (limit > 0 && count >= limit)
				goto leave_limit;

			if (feof(stdin))
				goto leave_eof;
		}

		goto leave;

leave_eof:
		logger->info("Reached end-of-file.");
		raise(SIGUSR1);
		goto leave;

leave_limit:
		logger->info("Reached send limit.");
		raise(SIGUSR1);

leave:
		logger->debug("Send thread stopped");
	}
};

class PipeReceiveDirection : public PipeDirection {

	friend Pipe;

public:
	PipeReceiveDirection(Node *n, Format *i, bool en = true, int lim = -1) :
		PipeDirection(n, i, en, lim, "recv")
	{ }

	virtual
	void run()
	{
		logger->debug("Receive thread started");

		int recv, allocated = 0;
		struct Sample *smps[node->in.vectorize];

		while (!stop) {
			allocated = sample_alloc_many(&pool, smps, node->in.vectorize);
			if (allocated < 0)
				throw RuntimeError("Failed to allocate {} samples from receive pool.", node->in.vectorize);
			else if (allocated < (int) node->in.vectorize)
				logger->warn("Receive pool underrun: allocated only {} of {} samples", allocated, node->in.vectorize);

			recv = node->read(smps, allocated);
			if (recv < 0) {
				if (node->getState() == State::STOPPING || stop) {
					sample_decref_many(smps, allocated);
					goto leave;
				}

				logger->warn("Failed to receive samples from node {}: reason={}", *node, recv);
			} else
				formatter->print(stdout, smps, recv);

			sample_decref_many(smps, allocated);

			count += recv;
			if (limit > 0 && count >= limit)
				goto leave_limit;
		}

		goto leave;

leave_limit:
		logger->info("Reached receive limit.");
		raise(SIGUSR1);

leave:
		logger->debug("Receive thread stopped");
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
		config_cli(json_object())
	{
		send.enabled = true;
		send.limit = -1;

		recv.enabled = true;
		recv.limit = -1;

		int ret = memory::init(DEFAULT_NR_HUGEPAGES);
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

	struct {
		int limit;
		bool enabled;
		std::unique_ptr<PipeReceiveDirection> dir;

	} recv;

	struct {
		int limit;
		bool enabled;
		std::unique_ptr<PipeSendDirection> dir;
	} send;

	void handler(int signal, siginfo_t *sinfo, void *ctx)
	{
		logger->debug("Received {} signal.", strsignal(signal));

		switch (signal)  {
			case SIGALRM:
				logger->info("Reached timeout.");
				stop = true;
				break;

			case SIGUSR1:
				if (recv.dir->enabled) {
					if (recv.dir->limit < 0 && feof(stdin))
						stop = true;

					if (recv.dir->limit > 0 && recv.dir->count >= recv.dir->limit)
						stop = true;
				}

				if (send.dir->enabled && send.dir->limit > 0) {
					if (send.dir->count >= send.dir->limit)
						stop = true;
				}
				break;

			default:
				stop = true;
				break;
		}
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
					recv.enabled = false; // send only
					break;

				case 'r':
					send.enabled = false; // receive only
					break;

				case 'l':
					recv.limit = strtoul(optarg, &endptr, 10);
					goto check;

				case 'L':
					send.limit = strtoul(optarg, &endptr, 10);
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
		Node *node;
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

		if (recv.enabled && !(node->getFactory()->getFlags() & (int) NodeFactory::Flags::SUPPORTS_READ))
			throw RuntimeError("Node {} can not receive data. Consider using send-only mode by using '-s' option", nodestr);

		if (send.enabled && !(node->getFactory()->getFlags() & (int) NodeFactory::Flags::SUPPORTS_WRITE))
				throw RuntimeError("Node {} can not send data. Consider using receive-only mode by using '-r' option", nodestr);

#if defined(WITH_NODE_WEBSOCKET) && defined(WITH_WEB)
		/* Only start web subsystem if villas-pipe is used with a websocket node */
		if (node->getFactory()->getFlags() & (int) NodeFactory::Flags::REQUIRES_WEB) {
			Web *w = sn.getWeb();
			w->start();
		}
#endif /* WITH_NODE_WEBSOCKET */

		if (reverse)
			node->reverse();

		ret = node->getFactory()->start(&sn);
		if (ret)
			throw RuntimeError("Failed to intialize node type {}: reason={}", *node->getFactory(), ret);

		sn.startInterfaces();

		ret = node->check();
		if (ret)
			throw RuntimeError("Invalid node configuration");

		ret = node->prepare();
		if (ret)
			throw RuntimeError("Failed to prepare node {}: reason={}", *node, ret);

		ret = node->start();
		if (ret)
			throw RuntimeError("Failed to start node {}: reason={}", *node, ret);

		recv.dir = std::make_unique<PipeReceiveDirection>(node, formatter, recv.enabled, recv.limit);
		send.dir = std::make_unique<PipeSendDirection>(node, formatter, send.enabled, send.limit);

		recv.dir->startThread();
		send.dir->startThread();

		/* Arm timeout timer */
		alarm(timeout);

		while (!stop)
			usleep(0.1e6);

		/* We are stopping the node here in order to unblock the receiving threads
		 * Node::read() call and allow it to be joined(). */
		ret = node->stop();
		if (ret)
			throw RuntimeError("Failed to stop node {}: reason={}", *node, ret);

		recv.dir->stopThread();
		send.dir->stopThread();

		sn.stopInterfaces();

		ret = node->getFactory()->stop();
		if (ret)
			throw RuntimeError("Failed to stop node type {}: reason={}", *node->getFactory(), ret);

#if defined(WITH_NODE_WEBSOCKET) && defined(WITH_WEB)
		/* Only start web subsystem if villas-pipe is used with a websocket node */
		if (node->getFactory()->getFlags() & (int) NodeFactory::Flags::REQUIRES_WEB) {
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
