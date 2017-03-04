/** VILLASfpga utility for tests and benchmarks
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 **********************************************************************************/
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>

#include <villas/log.h>
#include <villas/cfg.h>
#include <villas/timing.h>
#include <villas/utils.h>
#include <villas/nodes/fpga.h>
#include <villas/kernel/rt.h>
#include <villas/kernel/pci.h>
#include <villas/kernel/kernel.h>

#include <villas/fpga/card.h>

#include "config.h"

/* Declarations */
int fpga_benchmarks(int argc, char *argv[], struct fpga_card *c);
int fpga_tests(int argc, char *argv[], struct fpga_card *c);

struct cfg cfg;

void usage(char *name)
{
	printf("Usage: %s CONFIGFILE CARD CMD [OPTIONS]\n", name);
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
	struct fpga_card *card;

	enum {
		FPGA_TESTS,
		FPGA_BENCH
	} subcommand;

	if (argc < 4)
		usage(argv[0]);
	if      (strcmp(argv[3], "tests") == 0)
		subcommand = FPGA_TESTS;
	else if (strcmp(argv[3], "benchmarks") == 0)
		subcommand = FPGA_BENCH;
	else
		usage(argv[0]);

	/* Parse arguments */
	char c, *endptr;
	while ((c = getopt(argc-1, argv+1, "d:")) != -1) {
		switch (c) {
			case 'd':
				cfg.log.level = strtoul(optarg, &endptr, 10);
				break;	

			case '?':
			default:
				usage(argv[0]);
		}
	}

	info("Parsing configuration");
	cfg_parse(&cfg, argv[1]);
	
	info("Initialize real-time system");
	rt_init(&cfg);

	/* Initialize VILLASfpga card */
	ret = fpga_init(argc, argv, config_root_setting(cfg.cfg));
	if (ret)
		error("Failed to initialize FPGA card");
	
	card = fpga_lookup_card(argv[2]);
	if (!card)
		error("FPGA card '%s' does not exist", argv[2]);

	fpga_card_dump(card);

	/* Start subcommand */
	switch (subcommand) {
		case FPGA_TESTS: fpga_tests(argc-optind-1, argv+optind+1, card);      break;
		case FPGA_BENCH: fpga_benchmarks(argc-optind-1, argv+optind+1, card); break;
	}

	/* Shutdown */
	ret = fpga_deinit();
	if (ret)
		error("Failed to de-initialize FPGA card");
	
	cfg_destroy(&cfg);

	return 0;
}
