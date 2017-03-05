/** Receive messages from server snd print them on stdout.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *
 * @addtogroup tools Test and debug tools
 * @{
 *********************************************************************************/

#include <stdio.h>
#include <unistd.h>

#include <villas/sample.h>
#include <villas/hook.h>
#include <villas/utils.h>
#include <villas/pool.h>
#include <villas/log.h>
#include <villas/plugin.h>

#include <villas/kernel/rt.h>

#include "config.h"

static void usage()
{
	printf("Usage: villas-hook [OPTIONS] NAME [PARAMETER] \n");
	printf("  NAME      the name of the hook function to run\n");
	printf("  PARAM     the name of the node to which samples are sent and received from\n\n");
	printf("  OPTIONS are:\n");
	printf("    -h      show this help\n");
	printf("    -d lvl  set debug level\n");
	printf("    -v      process multiple samples at once\n\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	int j, ret, cnt = 1;
	
	char *name, *parameter;
	
	struct log log;
	struct hook *h;
	struct plugin *p;

	struct hook_info *i = alloc(sizeof(struct hook_info));
	
	char c;
	while ((c = getopt(argc, argv, "hv:d:")) != -1) {
		switch (c) {
			case 'v':
				cnt = atoi(optarg);
				break;
			case 'd':
				log.level = atoi(optarg);
				break;
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}
	}
	
	if (argc <= optind)
		
	name      = argc > optind     ? argv[optind  ] : NULL;
	parameter = argc > optind + 1 ? argv[optind+1] : NULL;
	
	if (argc > optind)
		name = argv[optind];
	else {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if (argc > optind + 1)
		parameter = argv[optind + 1];
	
	p = plugin_lookup(PLUGIN_TYPE_HOOK, name);
	if (!p)
		error("Unknown hook function '%s'", argv[optind]);
	
	h = &p->hook;
	
	if (cnt < 1)
		error("Vectorize option must be greater than 0");
	
	struct pool pool;
	struct sample *smps[cnt];

	info("Initialize logging system");
	log_init(&log, log.level, LOG_ALL);

	info("Initialize real-time system");
	rt_init(-1, 50);
	
	info("Initialize memory system");
	memory_init();
	
	ret = pool_init(&pool, 10 * cnt, SAMPLE_LEN(DEFAULT_VALUES), &memtype_hugepage);
	if (ret)
		error("Failed to initilize memory pool");

	ret = sample_alloc(&pool, smps, cnt);
	if (ret)
		error("Failed to allocate %u samples from pool", cnt);
	
	h->parameter = parameter;
	i->smps = smps;
	
	h->cb(h, HOOK_INIT, i);
	h->cb(h, HOOK_PARSE, i);
	h->cb(h, HOOK_PATH_START, i);
	
	while (!feof(stdin)) {
		for (j = 0; j < cnt && !feof(stdin); j++)
			sample_fscan(stdin, i->smps[j], NULL);
		
		i->cnt = j;
		i->cnt = h->cb(h, HOOK_READ, i);
		i->cnt = h->cb(h, HOOK_WRITE, i);
		
		for (j = 0; j < i->cnt; j++)
			sample_fprint(stdout, i->smps[j], SAMPLE_ALL);
	}
	
	h->cb(h, HOOK_PATH_STOP, i);
	h->cb(h, HOOK_DESTROY, i);
	
	sample_free(smps, cnt);
	
	pool_destroy(&pool);

	return 0;
}