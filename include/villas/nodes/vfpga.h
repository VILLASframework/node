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

#include <xilinx/xtmrctr.h>
#include <xilinx/xintc.h>
#include <xilinx/xllfifo.h>
#include <xilinx/xaxis_switch.h>
#include <xilinx/xaxidma.h>

#include "kernel/vfio.h"

#include "fpga/switch.h"
#include "fpga/model.h"
#include "fpga/dma.h"
#include "fpga/fifo.h"
#include "fpga/rtds_axis.h"

#include <pci/pci.h>

#include "nodes/vfpga.h"
#include "node.h"
#include "list.h"

#define VFPGA_BAR		0	/**< The Base Address Register which is mmap()ed to the User Space */

struct vfpga {
	struct pci_filter filter;

	enum {
		vfpga_MODE_FIFO,
		vfpga_MODE_DMA
	} mode;

	int features;

	struct list models;		/**< List of XSG and HLS model blocks on FPGA. */
	struct vfio_dev vd;		/**< VFIO device handle. */

	/* Xilinx IP blocks */
	XTmrCtr tmr;
	XLlFifo fifo;
	XAxiDma dma_simple;
	XAxiDma dma_sg;
	XAxis_Switch sw;

	char *map;			/**< PCI BAR0 mapping for register access */
	char *dma;			/**< DMA mapped memory */
	
	size_t maplen;
	size_t dmalen;
};

/** @see node_vtable::init */
int vfpga_init(int argc, char * argv[], config_setting_t *cfg);

/** @see node_vtable::deinit */
int vfpga_deinit();

/** @see node_vtable::parse */
int vfpga_parse(struct node *n, config_setting_t *cfg);

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

//////////

int vfpga_init2(struct vfpga *f, struct vfio_container *vc, struct pci_dev *pdev);

int vfpga_deinit2(struct vfpga *f);

int vfpga_reset(struct vfpga *f);

void vfpga_dump(struct vfpga *f);

#endif /** _VFPGA_H_ @} */
