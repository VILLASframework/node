/** Node type: GTFPGA (Xilinx ML507)
 *
 * This file implements the gtfpga subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 * @file
 * @addtogroup gtfpga RTDS - PCIexpress Fiber interface node type
 * @{
 */

#ifndef _GTFPGA_H_
#define _GTFPGA_H_

#include <pci/pci.h>

#define GTFPGA_BAR	0  /**< The Base Address Register which is mmap()ed to the User Space */

#define GTFPGA_MAX_TX	64 /**< The amount of values which is supported by the GTFPGA card */
#define GTFPGA_MAX_RX	64 /**< The amount of values which is supported by the GTFPGA card */

#define GTFPGA_VID 0x10ee  /**< The default vendor ID of the GTFPGA card */
#define GTFPGA_DID 0x0007  /**< The default device ID of the GTFPGA card */


struct gtfpga {
	struct pci_filter filter;

	int fd_mmap, fd_uio;
	void *map;

	struct pci_dev *dev;
};

int gtfpga_init(int argc, char * argv[]);
/** @see node_vtable::init */

/** @see node_vtable::deinit */
int gtfpga_deinit();

/** @see node_vtable::parse */
int gtfpga_parse(config_setting_t *cfg, struct node *n);

/** @see node_vtable::print */
int gtfpga_print(struct node *n, char *buf, int len);

/** @see node_vtable::open */
int gtfpga_open(struct node *n);

/** @see node_vtable::close */
int gtfpga_close(struct node *n);

/** @see node_vtable::read */
int gtfpga_read(struct node *n, struct msg *m);

/** @see node_vtable::write */
int gtfpga_write(struct node *n, struct msg *m);

#endif /** _GTFPGA_H_ @} */
