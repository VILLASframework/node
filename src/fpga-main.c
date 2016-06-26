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
#include <villas/timing.h>
#include <villas/utils.h>
#include <villas/nodes/fpga.h>
#include <villas/kernel/rt.h>
#include <villas/kernel/pci.h>
#include <villas/kernel/kernel.h>

#include "config.h"
#include "config-fpga.h"

/* Declarations */
int fpga_bench(struct fpga *f);
int fpga_tests(struct fpga *f);

void usage(char *name)
{
	printf("Usage: %s CONFIGFILE CMD [OPTIONS]\n", name);
	printf("   Commands:\n");
	printf("      tests    Test functionality of VILLASfpga card\n");
	printf("      bench    Do benchmarks\n\n");
	printf("   Options:\n");
	printf("      -d    Set log level\n\n");

	print_copyright();

	exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{
	int ret;
	struct fpga *fpga;
	enum {
		FPGA_TESTS,
		FPGA_BENCH
	} subcommand;
	config_t config;
	
	if (argc < 3)
		usage(argv[0]);

	if      (strcmp(argv[2], "tests") == 0)
		subcommand = FPGA_TESTS;
	else if (strcmp(argv[2], "bench") == 0)
		subcommand = FPGA_BENCH;
	else
		usage(argv[0]);

	/* Setup libconfig */
	config_init(&config);
	ret = config_read_file(&config, argv[1]);
	if (ret != CONFIG_TRUE) {
		error("Failed to parse configuration: %s in %s:%d",
			config_error_text(&config),
			config_error_file(&config) ? config_error_file(&config) : argv[1],
			config_error_line(&config)
		);
	}

	/* Parse arguments */
	char c, *endptr;
	while ((c = getopt (argc-1, argv+1, "d:")) != -1) {
		switch (c) {
			case 'd':
				log_setlevel(strtoul(optarg, &endptr, 10), ~0);
				break;	

			case '?':
			default:
				usage(argv[0]);
		}
	}

	info("Initialize real-time system");
	rt_init(settings.affinity, settings.priority);

	/* Initialize VILLASfpga card */
	config_setting_t *cfg_root = config_root_setting(&config);
	ret = fpga_init(argc, argv, cfg_root);
	if (ret)
		error("Failed to initialize fpga card");
	
	fpga = fpga_get();

	fpga_dump(fpga);

	/* Setup scheduler */
	cpu_set_t set = integer_to_cpuset(AFFINITY);

	ret = sched_setaffinity(0, sizeof(set), &set);
	if (ret)
		serror("Failed to pin thread");

	ret = sched_setscheduler(0, SCHED_FIFO, &(struct sched_param) { .sched_priority = PRIORITY });
	if (ret)
		serror("Failed to change scheduler");

	for (int i = 0; i < fpga->vd.irqs[VFIO_PCI_MSI_IRQ_INDEX].count; i++) {
		ret = kernel_irq_setaffinity(fpga->vd.msi_irqs[i], AFFINITY, NULL);
		if (ret)
			serror("Failed to change affinity of VFIO-MSI interrupt");
	}

	/* Start subcommand */
	switch (subcommand) {
		case FPGA_TESTS: fpga_tests(fpga); break;
		case FPGA_BENCH: /*fpga_bench(fpga);*/ break;
	}

	/* Shutdown */
	ret = fpga_deinit(&fpga);
	if (ret)
		error("Failed to de-initialize fpga card");

	return 0;
}
