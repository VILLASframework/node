/** Receive messages from server snd print them on stdout.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include <stdio.h>
#include <unistd.h>

#include <villas/timing.h>
#include <villas/sample.h>
#include <villas/io.h>
#include <villas/hook.h>
#include <villas/utils.h>
#include <villas/pool.h>
#include <villas/log.h>
#include <villas/plugin.h>
#include <config_helper.h>

#include <villas/kernel/rt.h>

#include "config.h"

int cnt;

struct sample **smps;
struct plugin *p;

struct log  l = { .state = STATE_DESTROYED };
struct pool q = { .state = STATE_DESTROYED };
struct hook h = { .state = STATE_DESTROYED };
struct io  io = { .state = STATE_DESTROYED };

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	int ret;

	ret = hook_stop(&h);
	if (ret)
		error("Failed to stop hook");

	ret = hook_destroy(&h);
	if (ret)
		error("Failed to destroy hook");

	sample_free(smps, cnt);

	ret = pool_destroy(&q);
	if (ret)
		error("Failed to destroy memory pool");

	info(CLR_GRN("Goodbye!"));

	exit(EXIT_SUCCESS);
}

static void usage()
{
	printf("Usage: villas-hook [OPTIONS] NAME [[PARAM1] [PARAM2] ...]\n");
	printf("  NAME      the name of the hook function\n");
	printf("  PARAM*    a string of configuration settings for the hook\n");
	printf("  OPTIONS is one or more of the following options:\n");
	printf("    -f FMT  the data format\n");
	printf("    -h      show this help\n");
	printf("    -d LVL  set debug level to LVL\n");
	printf("    -v CNT  process CNT smps at once\n");
	printf("\n");

	printf("The following hook functions are supported:\n");
	plugin_dump(PLUGIN_TYPE_HOOK);
	printf("\n");

	printf("Supported IO formats:\n");
	plugin_dump(PLUGIN_TYPE_IO);
	printf("\n");

	printf("Example:");
	printf("  villas-signal random | villas-hook skip_first seconds=10\n");
	printf("\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret, recv;
	char *format = "villas";

	/* Default values */
	cnt = 1;

	json_t *cfg_cli = json_object();

	char c, *endptr;
	while ((c = getopt(argc, argv, "hv:d:f:o:")) != -1) {
		switch (c) {
			case 'f':
				format = optarg;
				break;
			case 'v':
				cnt = strtoul(optarg, &endptr, 0);
				goto check;
			case 'd':
				l.level = strtoul(optarg, &endptr, 0);
				goto check;
			case 'o':
				ret = json_object_extend_str(cfg_cli, optarg);
				if (ret)
					error("Invalid option: %s", optarg);
				break;
			case '?':
			case 'h':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}

		continue;

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);

	}

	if (argc < optind + 1) {
		usage();
		exit(EXIT_FAILURE);
	}

	char *hook = argv[optind];

	ret = log_init(&l, l.level, LOG_ALL);
	if (ret)
		error("Failed to initialize log");

	ret = log_start(&l);
	if (ret)
		error("Failed to start log");

	ret = signals_init(quit);
	if (ret)
		error("Failed to intialize signals");

	if (cnt < 1)
		error("Vectorize option must be greater than 0");

	ret = memory_init(DEFAULT_NR_HUGEPAGES);
	if (ret)
		error("Failed to initialize memory");

	smps = alloc(cnt * sizeof(struct sample *));

	ret = pool_init(&q, 10 * cnt, SAMPLE_LEN(DEFAULT_SAMPLELEN), &memtype_hugepage);
	if (ret)
		error("Failed to initilize memory pool");

	/* Initialize IO */
	p = plugin_lookup(PLUGIN_TYPE_IO, format);
	if (!p)
		error("Unknown IO format '%s'", format);

	ret = io_init(&io, &p->io, IO_FORMAT_ALL);
	if (ret)
		error("Failed to initialize IO");

	ret = io_open(&io, NULL);
	if (ret)
		error("Failed to open IO");

	/* Initialize hook */
	p = plugin_lookup(PLUGIN_TYPE_HOOK, hook);
	if (!p)
		error("Unknown hook function '%s'", hook);

	ret = hook_init(&h, &p->hook, NULL);
	if (ret)
		error("Failed to initialize hook");

	ret = hook_parse(&h, cfg_cli);
	if (ret)
		error("Failed to parse hook config");

	ret = hook_start(&h);
	if (ret)
		error("Failed to start hook");

	for (;;) {
		if (io_eof(&io)) {
			killme(SIGTERM);
			pause();
		}

		ret = sample_alloc(&q, smps, cnt);
		if (ret != cnt)
			error("Failed to allocate %d smps from pool", cnt);

		recv = io_scan(&io, smps, cnt);
		if (recv < 0)
			killme(SIGTERM);

		debug(15, "Read %u smps from stdin", recv);

		hook_read(&h, smps, (unsigned *) &recv);
		hook_process(&h, smps, (unsigned *) &recv);
		hook_write(&h, smps, (unsigned *) &recv);

		io_print(&io, smps, recv);

		sample_free(smps, cnt);
	}

	return 0;
}
