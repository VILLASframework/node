/** Node type: GTFPGA (Xilinx ML507)
 *
 * This file implements the gtfpga subtype for nodes.
 * It's based on the uio_pci_generic kernel module.
 * A detailed description of that module is available here:
 * http://www.hep.by/gnu/kernel/uio-howto/uio_pci_generic.html
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */
/**
 * @addtogroup gtfpga RTDS - PCIexpress Fiber interface node type
 * @ingroup node
 * @{
 *********************************************************************************/

#ifndef _GTFPGA_H_
#define _GTFPGA_H_

#include <pci/pci.h>

#include "node.h"

#define GTFPGA_BAR	0	/**< The Base Address Register which is mmap()ed to the User Space */

#define GTFPGA_MAX_TX	64	/**< The amount of values which is supported by the GTFPGA card */
#define GTFPGA_MAX_RX	64	/**< The amount of values which is supported by the GTFPGA card */

#define GTFPGA_VID	0x10ee	/**< The default vendor ID of the GTFPGA card */
#define GTFPGA_DID	0x0007	/**< The default device ID of the GTFPGA card */

struct gtfpga {
	struct pci_filter filter;

	int fd_irq;		/**< File descriptor for the timer. This is blocking when read(2) until a new IRQ / timer expiration. */

	char *map_bar0;		/**< Memory-mapped address of BAR0 */
	
	enum {
		GTFPGA_MODE_FIFO,
		GTFPGA_MODE_DMA
	} mode;

	char *name;
	double rate;

	struct pci_dev *dev;
};

typedef void(*log_cb_t)(char *, ...);

/** @see node_vtable::init */
int gtfpga_init(int argc, char * argv[], config_setting_t *cfg);

/** @see node_vtable::deinit */
int gtfpga_deinit();

/** @see node_vtable::parse */
int gtfpga_parse(struct node *n, config_setting_t *cfg);

/** @see node_vtable::print */
char * gtfpga_print(struct node *n);

/** @see node_vtable::open */
int gtfpga_open(struct node *n);

/** @see node_vtable::close */
int gtfpga_close(struct node *n);

/** @see node_vtable::read */
int gtfpga_read(struct node *n, struct pool *pool, int cnt);

/** @see node_vtable::write */
int gtfpga_write(struct node *n, struct pool *pool, int cnt);

#endif /** _GTFPGA_H_ @} */
