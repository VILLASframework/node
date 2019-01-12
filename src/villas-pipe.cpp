/** Receive messages from server snd print them on stdout.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <iostream>
#include <atomic>

#include <villas/node/config.h>
#include <villas/config_helper.h>
#include <villas/super_node.hpp>
#include <villas/copyright.hpp>
#include <villas/utils.hpp>
#include <villas/utils.h>
#include <villas/log.hpp>
#include <villas/node.h>
#include <villas/timing.h>
#include <villas/pool.h>
#include <villas/io.h>
#include <villas/kernel/rt.hpp>
#include <villas/exceptions.hpp>
#include <villas/format_type.h>
#include <villas/nodes/websocket.h>

using namespace villas;
using namespace villas::node;

class Direction {

public:
	Direction(struct node *n, struct io *i, bool en = true, int lim = -1) :
		node(n),
		io(i),
		enabled(en),
		limit(lim)
	{
		pool.state = STATE_DESTROYED;
		pool.queue.state = STATE_DESTROYED;

		/* Initialize memory */


		/* Initialize memory */
		unsigned pool_size = node_type(node)->pool_size ? node_type(node)->pool_size : LOG2_CEIL(node->out.vectorize);

		int ret = pool_init(&pool, pool_size, SAMPLE_LENGTH(DEFAULT_SAMPLE_LENGTH), node_memory_type(node, &memory_hugepage));
		if (ret < 0)
			throw RuntimeError("Failed to allocate memory for pool.");
	}

	Direction(const Direction &c)
	{
		io = c.io;
	}

	~Direction()
	{
		pool_destroy(&pool);
	}

	struct pool pool;
	struct node *node;
	struct io *io;

	pthread_t thread;

	bool enabled;
	int limit;
};

struct Directions {
	Direction send;
	Direction recv;
};

static std::atomic<bool> stop(false);

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	Logger logger = logging.get("pipe");

	if (signal == SIGALRM)
		logger->info("Reached timeout. Terminating...");

	stop = true;
}

static void usage()
{
	std::cout << "Usage: villas-pipe [OPTIONS] CONFIG NODE" << std::endl
	          << "  CONFIG  path to a configuration file" << std::endl
	          << "  NODE    the name of the node to which samples are sent and received from" << std::endl
	          << "  OPTIONS are:" << std::endl
	          << "    -f FMT           set the format" << std::endl
	          << "    -o OPTION=VALUE  overwrite options in config file" << std::endl
	          << "    -x               swap read / write endpoints" << std::endl
	          << "    -s               only read data from stdin and send it to node" << std::endl
	          << "    -r               only read data from node and write it to stdout" << std::endl
	          << "    -t NUM           terminate after NUM seconds" << std::endl
	          << "    -L NUM           terminate after NUM samples sent" << std::endl
	          << "    -l NUM           terminate after NUM samples received" << std::endl
	          << "    -h               show this usage information" << std::endl
	          << "    -d               set logging level" << std::endl
	          << "    -V               show the version of the tool" << std::endl << std::endl;

	print_copyright();
}

static void * send_loop(void *ctx)
{
	Directions *dirs = static_cast<Directions*>(ctx);
	Logger logger = logging.get("pipe");

	unsigned last_sequenceno = 0, release;
	int scanned, sent, allocated, cnt = 0;

	struct sample *smps[dirs->send.node->out.vectorize];

	while (!io_eof(dirs->send.io)) {
		allocated = sample_alloc_many(&dirs->send.pool, smps, dirs->send.node->out.vectorize);
		if (allocated < 0)
			throw RuntimeError("Failed to get {} samples out of send pool.", dirs->send.node->out.vectorize);
		else if (allocated < dirs->send.node->out.vectorize)
			logger->warn("Send pool underrun");

		scanned = io_scan(dirs->send.io, smps, allocated);
		if (scanned < 0) {
			logger->warn("Failed to read samples from stdin");
			continue;
		}
		else if (scanned == 0)
			continue;

		/* Fill in missing sequence numbers */
		for (int i = 0; i < scanned; i++) {
			if (smps[i]->flags & SAMPLE_HAS_SEQUENCE)
				last_sequenceno = smps[i]->sequence;
			else
				smps[i]->sequence = last_sequenceno++;
		}

		release = allocated;

		sent = node_write(dirs->send.node, smps, scanned, &release);
		if (sent < 0)
			logger->warn("Failed to sent samples to node {}: reason={}", node_name(dirs->send.node), sent);
		else if (sent < scanned)
			logger->warn("Failed to sent {} out of {} samples to node {}", scanned-sent, scanned, node_name(dirs->send.node));

		sample_decref_many(smps, release);

		cnt += sent;
		if (dirs->send.limit > 0 && cnt >= dirs->send.limit)
			goto leave;

		pthread_testcancel();
	}

leave:	if (io_eof(dirs->send.io)) {
		if (dirs->recv.limit < 0) {
			logger->info("Reached end-of-file. Terminating...");
			killme(SIGTERM);
		}
		else
			logger->info("Reached end-of-file. Wait for receive side...");
	}
	else {
		logger->info("Reached send limit. Terminating...");
		killme(SIGTERM);
	}

	return nullptr;
}

static void * recv_loop(void *ctx)
{
	Directions *dirs = static_cast<Directions*>(ctx);
	Logger logger = logging.get("pipe");

	int recv, cnt = 0, allocated = 0;
	unsigned release;
	struct sample *smps[dirs->recv.node->in.vectorize];

	for (;;) {
		allocated = sample_alloc_many(&dirs->recv.pool, smps, dirs->recv.node->in.vectorize);
		if (allocated < 0)
			throw RuntimeError("Failed to allocate {} samples from receive pool.", dirs->recv.node->in.vectorize);
		else if (allocated < dirs->recv.node->in.vectorize)
			logger->warn("Receive pool underrun: allocated only {} of {} samples", allocated, dirs->recv.node->in.vectorize);

		release = allocated;

		recv = node_read(dirs->recv.node, smps, allocated, &release);
		if (recv < 0)
			logger->warn("Failed to receive samples from node {}: reason={}", node_name(dirs->recv.node), recv);
		else {
			io_print(dirs->recv.io, smps, recv);

			cnt += recv;
			if (dirs->recv.limit > 0 && cnt >= dirs->recv.limit)
				goto leave;
		}

		sample_decref_many(smps, release);
		pthread_testcancel();
	}

leave:	logger->info("Reached receive limit. Terminating...");
	killme(SIGTERM);

	return nullptr;
}

int main(int argc, char *argv[])
{
	int ret, timeout = 0;
	bool reverse = false;
	const char *format = "villas.human";

	struct node *node;
	static struct io io = { .state = STATE_DESTROYED };

	SuperNode sn; /**< The global configuration */
	Logger logger = logging.get("pipe");

	json_t *cfg_cli = json_object();

	bool enable_send = true, enable_recv = true;
	int limit_send = -1, limit_recv = -1;

	/* Parse optional command line arguments */
	int c;
	char *endptr;
	while ((c = getopt(argc, argv, "Vhxrsd:l:L:t:f:o:")) != -1) {
		switch (c) {
			case 'V':
				print_version();
				exit(EXIT_SUCCESS);

			case 'f':
				format = optarg;
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

			case 't':
				timeout = strtoul(optarg, &endptr, 10);
				goto check;

			case 'o':
				ret = json_object_extend_str(cfg_cli, optarg);
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

check:		if (optarg == endptr)
			throw RuntimeError("Failed to parse parse option argument '-{} {}'", c, optarg);
	}

	if (argc != optind + 2) {
		usage();
		exit(EXIT_FAILURE);
	}

	logger->info("Logging level: {}", logging.getLevelName());

	char *uri = argv[optind];
	char *nodestr = argv[optind+1];
	struct format_type *fmt;

	ret = memory_init(0);
	if (ret)
		throw RuntimeError("Failed to intialize memory");

	ret = utils::signals_init(quit);
	if (ret)
		throw RuntimeError("Failed to initialize signals");

	if (uri) {
		ret = sn.parseUri(uri);
		if (ret)
			throw RuntimeError("Failed to parse configuration");
	}
	else
		logger->warn("No configuration file specified. Starting unconfigured. Use the API to configure this instance.");

	fmt = format_type_lookup(format);
	if (!fmt)
		throw RuntimeError("Invalid format: {}", format);

	ret = io_init_auto(&io, fmt, DEFAULT_SAMPLE_LENGTH, SAMPLE_HAS_ALL);
	if (ret)
		throw RuntimeError("Failed to initialize IO");

	ret = io_check(&io);
	if (ret)
		throw RuntimeError("Failed to validate IO configuration");

	ret = io_open(&io, nullptr);
	if (ret)
		throw RuntimeError("Failed to open IO");

	node = sn.getNode(nodestr);
	if (!node)
		throw RuntimeError("Node {} does not exist!", nodestr);

#ifdef LIBWEBSOCKETS_FOUND
	/* Only start web subsystem if villas-pipe is used with a websocket node */
	if (node_type(node)->start == websocket_start) {
		Web *w = sn.getWeb();
		Api *a = sn.getApi();

		w->start();
		a->start();
	}
#endif /* LIBWEBSOCKETS_FOUND */

	if (reverse)
		node_reverse(node);

	ret = node_type_start(node_type(node), reinterpret_cast<super_node *>(&sn));
	if (ret)
		throw RuntimeError("Failed to intialize node type {}: reason={}", node_type_name(node_type(node)), ret);

	ret = node_check(node);
	if (ret)
		throw RuntimeError("Invalid node configuration");

	ret = node_init2(node);
	if (ret)
		throw RuntimeError("Failed to start node {}: reason={}", node_name(node), ret);

	ret = node_start(node);
	if (ret)
		throw RuntimeError("Failed to start node {}: reason={}", node_name(node), ret);

	/* Start threads */
	Directions dirs = {
		.send = Direction(node, &io, enable_send, limit_send),
		.recv = Direction(node, &io, enable_recv, limit_recv)
	};

	if (dirs.recv.enabled) {
		dirs.recv.node = node;
		pthread_create(&dirs.recv.thread, nullptr, recv_loop, &dirs);
	}

	if (dirs.send.enabled) {
		dirs.send.node = node;
		pthread_create(&dirs.send.thread, nullptr, send_loop, &dirs);
	}

	alarm(timeout);

	while (!stop)
		pause();

	if (dirs.recv.enabled) {
		pthread_cancel(dirs.recv.thread);
		pthread_join(dirs.recv.thread, nullptr);
	}

	if (dirs.send.enabled) {
		pthread_cancel(dirs.send.thread);
		pthread_join(dirs.send.thread, nullptr);
	}

	ret = node_stop(node);
	if (ret)
		throw RuntimeError("Failed to stop node {}: reason={}", node_name(node), ret);

	ret = node_type_stop(node->_vt);
	if (ret)
		throw RuntimeError("Failed to stop node type {}: reason={}", node_type_name(node->_vt), ret);

	ret = io_close(&io);
	if (ret)
		throw RuntimeError("Failed to close IO");

	ret = io_destroy(&io);
	if (ret)
		throw RuntimeError("Failed to destroy IO");

	logger->info(CLR_GRN("Goodbye!"));

	return 0;
}

/** @} */
