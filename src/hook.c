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
#include <villas/sample_io.h>
#include <villas/hook.h>
#include <villas/utils.h>
#include <villas/pool.h>
#include <villas/log.h>
#include <villas/plugin.h>
#include <config_helper.h>

#include <villas/kernel/rt.h>

#include "config.h"

int cnt;

struct sample **samples;
struct plugin *p;

struct log  l = { .state = STATE_DESTROYED };
struct pool q = { .state = STATE_DESTROYED };
struct hook h = { .state = STATE_DESTROYED };

static void quit(int signal, siginfo_t *sinfo, void *ctx)
{
	int ret;

	ret = hook_stop(&h);
	if (ret)
		error("Failed to stop hook");

	ret = hook_destroy(&h);
	if (ret)
		error("Failed to destroy hook");

	sample_free(samples, cnt);

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
	printf("    -h      show this help\n");
	printf("    -d LVL  set debug level to LVL\n");
	printf("    -v CNT  process CNT samples at once\n");
	printf("\n");
	printf("The following hook functions are supported:\n");
	plugin_dump(PLUGIN_TYPE_HOOK);
	printf("\n");
	printf("Example:");
	printf("  villas-signal random | villas-hook skip_first seconds=10\n");
	printf("\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;

	size_t recv;

	/* Default values */
	cnt = 1;

	json_t *cfg_cli = json_object();

	char c, *endptr;
	while ((c = getopt(argc, argv, "hv:d:f:o:")) != -1) {
		switch (c) {
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

	char *hookstr = argv[optind];

	ret = signals_init(quit);
	if (ret)
		error("Failed to intialize signals");

	ret = log_init(&l, l.level, LOG_ALL);
	if (ret)
		error("Failed to initialize log");

	log_start(&l);

	if (cnt < 1)
		error("Vectorize option must be greater than 0");

	memory_init(DEFAULT_NR_HUGEPAGES);

	samples = alloc(cnt * sizeof(struct sample *));

	ret = pool_init(&q, 10 * cnt, SAMPLE_LEN(DEFAULT_SAMPLELEN), &memtype_hugepage);
	if (ret)
		error("Failed to initilize memory pool");

	p = plugin_lookup(PLUGIN_TYPE_HOOK, hookstr);
	if (!p)
		error("Unknown hook function '%s'", hookstr);

	/** @todo villas-hook does not use the path structure */
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
		if (feof(stdin)) {
			killme(SIGTERM);
			pause();
		}

		ret = sample_alloc(&q, samples, cnt);
		if (ret != cnt)
			error("Failed to allocate %d samples from pool", cnt);

		recv = 0;
		for (int j = 0; j < cnt && !feof(stdin); j++) {
			ret = sample_io_villas_fscan(stdin, samples[j], NULL);
			if (ret < 0)
				break;

			samples[j]->ts.received = time_now();
			recv++;
		}

		debug(15, "Read %zu samples from stdin", recv);

		hook_read(&h, samples, &recv);
		hook_write(&h, samples, &recv);

		for (int j = 0; j < recv; j++)
			sample_io_villas_fprint(stdout, samples[j], SAMPLE_IO_ALL);
		fflush(stdout);

		sample_free(samples, cnt);
	}

	return 0;
}
