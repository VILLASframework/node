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

config_t cfg;

static int hook_parse_cli(struct hook *h, char *params[], int paramlen)
{
	int ret;
	char *str;
	config_setting_t *cfg_root;
	
	/* Concat all params */
	str = NULL;
	for (int i = 0; i < paramlen; i++)
		str = strcatf(&str, params[i]);

	config_set_auto_convert(&cfg, 1);

	if (str) {
		ret = config_read_string(&cfg, str);
		if (ret != CONFIG_TRUE)
			error("Failed to parse argument '%s': %s", str, config_error_text(&cfg));
	}
	
	//config_write(&cfg, stdout);

	cfg_root = config_root_setting(&cfg);
	ret = hook_parse(h, cfg_root);
	if (ret)
		error("Failed to parse hook settings");

	free(str);

	return 0;
}

static void usage()
{
	printf("Usage: villas-hook [OPTIONS] NAME [PARAM] \n");
	printf("  NAME      the name of the hook function to run\n");
	printf("  PARAM     a string of configuration settings for the hook\n\n");
	printf("  OPTIONS are:\n");
	printf("    -h      show this help\n");
	printf("    -d LVL  set debug level to LVL\n");
	printf("    -v CNT  process CNT samples at once\n");
	printf("\n");
	printf("Example:");
	printf("  villas-signal random | villas-hook skip_first seconds=10\n");
	printf("\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	int j, ret, level, cnt;
	
	/* Default values */
	level = V;
	cnt = 1;
	
	char *name;
	
	struct log log;
	struct plugin *p;
	struct sample *samples[cnt];
	struct pool pool = { .state = STATE_DESTROYED };
	struct hook h = { .state = STATE_DESTROYED };
	struct hook_info hi = { .samples = samples };

	char c;
	while ((c = getopt(argc, argv, "hv:d:")) != -1) {
		switch (c) {
			case 'v':
				cnt = atoi(optarg);
				break;
			case 'd':
				level = atoi(optarg);
				break;
			case 'h':
			case '?':
				usage();
				exit(c == '?' ? EXIT_FAILURE : EXIT_SUCCESS);
		}
	}
	
	log_init(&log, level, LOG_ALL);
	memory_init(DEFAULT_NR_HUGEPAGES);

	if (argc < optind + 1) {
		usage();
		exit(EXIT_FAILURE);
	}
	
	if (cnt < 1)
		error("Vectorize option must be greater than 0");
	
	ret = pool_init(&pool, 10 * cnt, SAMPLE_LEN(DEFAULT_VALUES), &memtype_hugepage);
	if (ret)
		error("Failed to initilize memory pool");
	
	name        = argv[optind];

	p = plugin_lookup(PLUGIN_TYPE_HOOK, name);
	if (!p)
		error("Unknown hook function '%s'", argv[optind]);

	config_init(&cfg);
	
	hook_init(&h, &p->hook, NULL);
	hook_parse_cli(&h, &argv[optind + 1], argc - optind - 1);
	hook_run(&h, HOOK_PATH_START, &hi);
	
	while (!feof(stdin)) {
		ret = sample_alloc(&pool, samples, cnt);
		if (ret != cnt)
			error("Failed to allocate %u samples from pool", cnt);

		for (j = 0; j < cnt && !feof(stdin); j++) {
			ret = sample_fscan(stdin, hi.samples[j], NULL);
			if (ret < 0)
				break;
		}
		
		debug(15, "Read %d samples from stdin", cnt);
		
		hi.count = j;
		
		if (h._vt->when & HOOK_READ)
			hi.count = h._vt->cb(&h, HOOK_READ, &hi);
		if (h._vt->when & HOOK_WRITE)
			hi.count = h._vt->cb(&h, HOOK_WRITE, &hi);
		
		for (j = 0; j < hi.count; j++)
			sample_fprint(stdout, hi.samples[j], SAMPLE_ALL);
		
		sample_free(samples, cnt);
	}
	
	hook_run(&h, HOOK_PATH_STOP, &hi);

	hook_destroy(&h);
	config_destroy(&cfg);
	
	sample_free(samples, cnt);
	pool_destroy(&pool);

	return 0;
}