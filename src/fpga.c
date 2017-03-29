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

#include "config.h"

/* Declarations */
int fpga_benchmarks(int argc, char *argv[], struct fpga_card *c);

void usage()
{
	printf("Usage: villas-fpga CONFIGFILE CARD [OPTIONS]\n\n");
	printf("   Options:\n");
	printf("      -d    Set log level\n\n");

	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;
	
	struct super_node sn;
	struct fpga_card *card;

	if (argc < 3) {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	/* Parse arguments */
	char c, *endptr;
	while ((c = getopt(argc-1, argv+1, "d:")) != -1) {
		switch (c) {
			case 'd':
				sn.log.level = strtoul(optarg, &endptr, 10);
				break;	

			case '?':
			default:
				usage();
				exit(EXIT_SUCCESS);
		}
	}

	super_node_init(&sn);
	super_node_parse_uri(&sn, argv[1]);
	
	log_init(&sn.log, sn.log.level, sn.log.facilities);
	rt_init(sn.priority, sn.affinity);
	memory_init(sn.hugepages);

	/* Initialize VILLASfpga card */
	ret = fpga_init(argc, argv, config_root_setting(&sn.cfg));
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
