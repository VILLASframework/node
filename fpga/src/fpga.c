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
#include <villas/utils.h>

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
	printf("    -h      show this help\n");
	printf("    -V      show the version of the tool\n");
	printf("\n");
	print_copyright();
}

int main(int argc, char *argv[])
{
	int ret;

	struct list cards;
	struct vfio_container vc;
	struct pci pci;
	struct fpga_card *card;

	/* Parse arguments */
	char c, *endptr;
	while ((c = getopt(argc, argv, "Vh")) != -1) {
		switch (c) {
			case 'V':
				print_version();
				exit(EXIT_SUCCESS);

			case 'h':
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
	char *cardname = argv[optind+1];

	FILE *f;
	json_error_t err;
	json_t *json;

	ret = pci_init(&pci);
	if (ret)
		return -1;

	ret = vfio_init(&vc);
	if (ret)
		return -1;

	/* Parse FPGA configuration */
	f = fopen(configfile, "r");
	if (!f)
		return -1;

	json = json_loadf(f, 0, &err);
	if (!json)
		return -1;

	fclose(f);

	list_init(&cards);
	ret = fpga_card_parse_list(&cards, json);
	if (ret)
		return -1;

	json_decref(json);

	card = list_lookup(&cards, cardname);
	if (!card)
		return -1;

	fpga_card_dump(card);

	/* Run benchmarks */
	fpga_benchmarks(argc-optind-1, argv+optind+1, card);

	return 0;
}
