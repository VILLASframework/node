/** VILLASfpga utility for tests and benchmarks
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/
 
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <sched.h>

#include <villas/log.h>
#include <villas/cfg.h>
#include <villas/timing.h>
#include <villas/utils.h>
#include <villas/nodes/fpga.h>
#include <villas/kernel/rt.h>
#include <villas/kernel/pci.h>
#include <villas/kernel/kernel.h>

#include "config.h"
#include "config-fpga.h"

/* Declarations */
int fpga_benchmarks(int argc, char *argv[], struct fpga *f);
int fpga_tests(int argc, char *argv[], struct fpga *f);

struct settings settings;

void usage(char *name)
{
	printf("Usage: %s CONFIGFILE CMD [OPTIONS]\n", name);
	printf("   Commands:\n");
	printf("      tests      Test functionality of VILLASfpga card\n");
	printf("      benchmarks Do benchmarks\n\n");
	printf("   Options:\n");
	printf("      -d    Set log level\n\n");

	print_copyright();

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int ret;
	struct fpga *fpga;
	config_t config;

	enum {
		FPGA_TESTS,
		FPGA_BENCH
	} subcommand;

	if (argc < 3)
		usage(argv[0]);
	if      (strcmp(argv[2], "tests") == 0)
		subcommand = FPGA_TESTS;
	else if (strcmp(argv[2], "benchmarks") == 0)
		subcommand = FPGA_BENCH;
	else
		usage(argv[0]);

	/* Parse arguments */
	char c, *endptr;
	while ((c = getopt(argc-1, argv+1, "d:")) != -1) {
		switch (c) {
			case 'd':
				log_setlevel(strtoul(optarg, &endptr, 10), ~0);
				break;	

			case '?':
			default:
				usage(argv[0]);
		}
	}

	info("Parsing configuration");
	cfg_parse(argv[1], &config, &settings, NULL, NULL);
	
	info("Initialize real-time system");
	rt_init(settings.affinity, settings.priority);

	/* Initialize VILLASfpga card */
	config_setting_t *cfg_root = config_root_setting(&config);
	ret = fpga_init(argc, argv, cfg_root);
	if (ret)
		error("Failed to initialize FPGA card");
	
	fpga = fpga_get();
	fpga_dump(fpga);

	/* Start subcommand */
	switch (subcommand) {
		case FPGA_TESTS: fpga_tests(argc-optind-1, argv+optind+1, fpga);      break;
		case FPGA_BENCH: fpga_benchmarks(argc-optind-1, argv+optind+1, fpga); break;
	}

	/* Shutdown */
	ret = fpga_deinit(&fpga);
	if (ret)
		error("Failed to de-initialize fpga card");
	
	cfg_destroy(&config);

	return 0;
}
