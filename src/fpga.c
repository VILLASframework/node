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
#include <villas/super_node.h>
#include <villas/timing.h>
#include <villas/utils.h>
#include <villas/memory.h>
#include <villas/nodes/fpga.h>
#include <villas/kernel/rt.h>
#include <villas/kernel/pci.h>
#include <villas/kernel/kernel.h>

#include <villas/fpga/card.h>

/* Declarations */
int fpga_benchmarks(int argc, char *argv[], struct fpga_card *c);

void usage()
{
	printf("Usage: villas-fpga [OPTIONS] CONFIG CARD\n\n");
	printf("  CONFIG  path to a configuration file\n");
	printf("  CARD    name of the FPGA card\n");
	printf("  OPTIONS is one or more of the following options:\n");
	printf("     -d    Set log level\n");
	printf("\n");
	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;
	
	struct super_node sn;
	struct fpga_card *card;

	/* Parse arguments */
	char c, *endptr;
	while ((c = getopt(argc, argv, "d:")) != -1) {
		switch (c) {
			case 'd':
				sn.log.level = strtoul(optarg, &endptr, 10);
				goto check;

			case '?':
			default:
				usage();
				exit(EXIT_SUCCESS);
		}

check:		if (optarg == endptr)
			error("Failed to parse parse option argument '-%c %s'", c, optarg);
	}
	
	if (argc != optind + 2) {
		usage();
		exit(EXIT_FAILURE);
	}
	
	char *configfile = argv[optind];

	super_node_init(&sn);
	super_node_parse_uri(&sn, configfile);
	
	log_init(&sn.log, sn.log.level, sn.log.facilities);
	rt_init(sn.priority, sn.affinity);
	memory_init(sn.hugepages);

	/* Initialize VILLASfpga card */
	ret = fpga_init(&sn);
	if (ret)
		error("Failed to initialize FPGA card");
	
	card = fpga_lookup_card(argv[2]);
	if (!card)
		error("FPGA card '%s' does not exist", argv[2]);

	fpga_card_dump(card);

	/* Run benchmarks */
	fpga_benchmarks(argc-optind-1, argv+optind+1, card);

	/* Shutdown */
	ret = fpga_deinit();
	if (ret)
		error("Failed to de-initialize FPGA card");
	
	super_node_destroy(&sn);

	return 0;
}
