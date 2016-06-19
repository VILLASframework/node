/** Hardcoded configuration for VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#ifndef _CONFIG_FPGA_H_
#define _CONFIG_FPGA_H_

#define AFFINITY		(1 << 3)
#define PRIORITY		90

#define RTDS_DM_FIFO		1
#define RTDS_DM_DMA_SIMPLE	2
#define RTDS_DM_DMA_SG		3
#define DATAMOVER		RTDS_DM_DMA_SIMPLE

/** PCIe BAR number of VILLASfpga registers */
#define PCI_BAR			0

/** AXI Bus frequency for all components
 * except RTDS AXI Stream bridge which runs at RTDS_HZ (100 Mhz) */
#define AXI_HZ			125000000 // 125 MHz

#define PCI_VID_XILINX		0x10ee
#define PCI_PID_VFPGA		0x7022

#endif /* _CONFIG_FPGA_H_ */