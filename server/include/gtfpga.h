/** Node type: GTFPGA (Xilinx ML507)
 *
 * This file implements the gtfpga subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#ifndef _GTFPGA_H_
#define _GTFPGA_H_

#include <pci/pci.h>

#define GTFPGA_BAR	0

#define GTFPGA_MAX_TX	64
#define GTFPGA_MAX_RX	64

#define GTFPGA_VID 0x10ee
#define GTFPGA_DID 0x0007

struct gtfpga {
	struct pci_filter filter;

	int fd_mmap, fd_uio;
	void *map;

	struct pci_access *pacc;
	struct pci_dev *dev;
};


int gtfpga_open(struct node *n);

int gtfpga_close(struct node *n);

int gtfpga_read(struct node *n, struct msg *m);

int gtfpga_write(struct node *n, struct msg *m);

#endif /* _GTFPGA_H_ */
