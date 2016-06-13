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
#include <dirent.h>
#include <errno.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <getopt.h>
#include <sched.h>

#include <villas/log.h>
#include <villas/timing.h>
#include <villas/utils.h>
#include <villas/nodes/vfpga.h>
#include <villas/kernel/pci.h>
#include <villas/kernel/kernel.h>

#include "config.h"
#include "config-fpga.h"

#include "fpga-tests.h"
#include "fpga-bench.h"

static struct pci_access *pacc;

void usage(char *name)
{
	printf("Usage: %s [ARGS]\n", name);
	printf("  -d    Set log level\n");
	printf("  -i    ID pair of PCI device: [vendor]:[device][:class]\n");
	printf("  -s    Slot of PCI device: [[[domain]:][bus]:][slot][.[func]]\n\n");
	printf("fpga PCIutil %s (built on %s %s)\n",
		VERSION, __DATE__, __TIME__);
	printf(" copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC\n");
	printf(" Steffen Vogel <StVogel@eonerc.rwth-aachen.de>\n");
}

int main(int argc, char *argv[])
{
	int ret;
	char *err;

	struct vfpga fpga;
	struct vfio_container vc;
	struct pci_dev *pdev;
	struct pci_filter filter;

	/* Init libpci */
	pacc = pci_alloc();		/* Get the pci_access structure */
	pci_init(pacc);			/* Initialize the PCI library */
	pci_scan_bus(pacc);		/* We want to get the list of devices */
	pci_filter_init(pacc, &filter);

	/* Default filter */
	filter.vendor = PCI_VID_XILINX;
	filter.device = PCI_PID_VFPGA;
	
	/* Parse arguments */
	char c, *endptr;
	while ((c = getopt (argc, argv, "i:s:d:")) != -1) {
		switch (c) {
			case 's':
				err = pci_filter_parse_slot(&filter, optarg);
				if (err)
					error("Failed to parse slot: %s", err);
				break;

			case 'i':
				err = pci_filter_parse_id(&filter, optarg);
				if (err)
					error("Failed to parse ID: %s", err);
				break;
			
			case 'd':
				log_setlevel(strtoul(optarg, &endptr, 10), ~0);
				break;	

			case '?':
			default:
				usage(argv[0]);
		}
	}

	/* Search for fpga card */
	pdev = pci_find_device(pacc, &filter);
	if (!pdev)
		error("Failed to find PCI device");

	/* Get VFIO handles and details */
	ret = vfio_init(&vc);
	if (ret)
		serror("Failed to initialize VFIO");

	/* Initialize fpga card */
	ret = vfpga_init2(&fpga, &vc, pdev);
	if (ret)
		error("Failed to initialize fpga card");

	vfpga_dump(&fpga);

	/* Setup scheduler */
	cpu_set_t set = integer_to_cpuset(AFFINITY);

	ret = sched_setaffinity(0, sizeof(set), &set);
	if (ret)
		serror("Failed to pin thread");

	ret = sched_setscheduler(0, SCHED_FIFO, &(struct sched_param) { .sched_priority = PRIORITY });
	if (ret)
		serror("Failed to change scheduler");

	for (int i = 0; i < fpga.vd.irqs[VFIO_PCI_MSI_IRQ_INDEX].count; i++) {
		ret = kernel_irq_setaffinity(fpga.vd.msi_irqs[i], AFFINITY, NULL);
		if (ret)
			serror("Failed to change affinity of VFIO-MSI interrupt");
	}

	/* Start tests */
	ret = test_intc(&fpga);
	info("INTC Test: %s", (ret == 0) ? GRN("passed") : RED("failed"));

	ret = test_xsg(&fpga, "xsg_multiply_add");
	info("XSG Test: %s", (ret == 0) ? GRN("passed") : RED("failed"));

//	ret = test_timer(&fpga);
//	info("Timer Test: %s", (ret == 0) ? GRN("passed") : RED("failed"));

//	ret = test_fifo(&fpga);
//	info("FIFO Test: %s", (ret == 0) ? GRN("passed") : RED("failed"));

	ret = test_dma(&fpga);
	info("DMA Test: %s", (ret == 0) ? GRN("passed") : RED("failed"));

//	ret = test_rtds_rtt(&fpga);
//	info("RTDS RTT Test: %s", (ret == 0) ? GRN("passed") : RED("failed"));

//	ret = test_rtds_cbuilder(&fpga);
//	info("RTDS RTT Test: %s", (ret == 0) ? GRN("passed") : RED("failed"));

	/* Start benchmarks */
#if 0
	ret = bench_memcpy(&fpga);
#endif

	/* Shutdown */
	ret = vfpga_deinit2(&fpga);
	if (ret)
		error("Failed to de-initialize fpga card");

	ret = vfio_destroy(&vc);
	if (ret)
		error("Failed to deinitialize VFIO module");

	pci_cleanup(pacc);

	return 0;
}
