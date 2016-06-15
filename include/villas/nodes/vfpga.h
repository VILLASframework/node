/** Node type: VILLASfpga
 *
 * This file implements the vfpga node-type.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited. 
 */
/**
 * @addtogroup vfpga VILLASnode
 * @ingroup node
 * @{
 *********************************************************************************/

#ifndef _VFPGA_H_
#define _VFPGA_H_

#include "kernel/vfio.h"

#include <pci/pci.h>

#include "nodes/vfpga.h"
#include "node.h"
#include "list.h"

#define BASEADDR_HOST		0x8000000
#define VFPGA_BAR		0	/**< The Base Address Register which is mmap()ed to the User Space */

struct vfpga {
	struct pci_filter filter;	/**< Filter for libpci with device id & slot */
	struct vfio_dev vd;		/**< VFIO device handle. */

	int reset;			/**< Reset VILLASfpga during startup? */

	struct list ips;		/**< List of IP components on FPGA. */

	char *map;			/**< PCI BAR0 mapping for register access */
	char *dma;			/**< DMA mapped memory */

	size_t maplen;
	size_t dmalen;

	/** Base addresses for internal IP blocks */
	struct {
		uintptr_t reset;
		uintptr_t intc;
	} baseaddr;
};

/** @see node_vtable::init */
int vfpga_init(int argc, char * argv[], config_setting_t *cfg);

/** @see node_vtable::deinit */
int vfpga_deinit();

/** @see node_vtable::parse */
int vfpga_parse_node(struct node *n, config_setting_t *cfg);

int vfpga_parse(struct vfpga *v, int argc, char * argv[], config_setting_t *cfg);

/** @see node_vtable::print */
char * vfpga_print(struct node *n);

/** @see node_vtable::open */
int vfpga_open(struct node *n);

/** @see node_vtable::close */
int vfpga_close(struct node *n);

/** @see node_vtable::read */
int vfpga_read(struct node *n, struct sample *smps[], unsigned cnt);

/** @see node_vtable::write */
int vfpga_write(struct node *n, struct sample *smps[], unsigned cnt);

/** Get pointer to internal VILLASfpga datastructure */
struct vfpga * vfpga_get();

/** Reset VILLASfpga */
int vfpga_reset(struct vfpga *f);

/** Dump some details about the fpga card */
void vfpga_dump(struct vfpga *f);

#endif /** _VFPGA_H_ @} */
