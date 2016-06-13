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

#define virt_to_axi(virt, map) ((char *) ((char *) virt - map))
#define virt_to_dma(virt, dma) ((char *) ((char *) virt - dma) + BASEADDR_HOST)
#define dma_to_virt(addr, dma) ((char *) ((char *) addr - BASEADDR_HOST) + dma)

#if 0
/** Base addresses in AXI bus of FPGA */
enum {
	BASEADDR_BRAM		= 0x0000, /* 8 KB Block RAM for DMA SG descriptors */
	BASEADDR_XSG		= 0x2000, /* Xilinx System Generator Model */
	BASEADDR_DMA_SG		= 0x3000, /* Scatter Gather DMA controller registers */
	BASEADDR_TIMER		= 0x4000, /* Timer Counter registers */
	BASEADDR_SWITCH		= 0x5000, /* AXI Stream switch registers */
	BASEADDR_FIFO_MM	= 0x6000, /* Memory-mapped to AXI Stream FIFO */
	BASEADDR_RESET		= 0x7000, /* Internal Reset register */
	BASEADDR_RTDS		= 0x8000, /* RTDS to AXI Stream bridge */
	BASEADDR_HLS		= 0x9000, /* High-level Synthesis model registers */
	BASEADDR_DMA_SIMPLE	= 0xA000, /* Simple DMA controller registers */
	BASEADDR_INTC		= 0xB000, /* PCIe MSI Interrupt controller registers */
	BASEADDR_FIFO_MM_AXI4	= 0xC000, /* AXI4 (full) interface of AXI Stream FIFO */
	BASEADDR_HOST		= 0x80000000 /* Start of host memory (0x0 of IOMMU) */
};

/** MSI vector table for VILLASfpga components */
enum {
	MSI_TMRCTR0		= 0, /* Sensivity: rising edge, synchronous: axi_clk */
	MSI_TMRCTR1		= 1, /* Sensivity: rising edge, synchronous: axi_clk */
	MSI_FIFO_MM		= 2, /* Sensivity: level high,  synchronous: axi_clk */
	MSI_DMA_MM2S		= 3, /* Sensivity: level high,  synchronous: axi_clk */
	MSI_DMA_S2MM		= 4, /* Sensivity: level high,  synchronous: axi_clk */
	MSI_RTDS_TS		= 5, /* Sensivity: rising edge, synchronous: rtds_clk100m (New timestep started) */
	MSI_RTDS_OVF		= 6, /* Sensivity: rising edge, synchronous: rtds_clk100m (UserTxInProgress overlapped with UserTStepPulse)*/
	MSI_RTDS_CASE		= 7  /* Sensivity: rising edge, synchronous: rtds_clk100m (New RSCAD case started) */
};

/** Mapping of VILLASfpga components to AXI Stream interconnect ports */
enum {
	AXIS_SWITCH_RTDS	= 0,
	AXIS_SWITCH_DMA_SG	= 1,
	AXIS_SWITCH_FIFO_MM	= 2,
	AXIS_SWITCH_FIFO0	= 3,
	AXIS_SWITCH_XSG		= 4,
	AXIS_SWITCH_HLS		= 5,
	AXIS_SWITCH_FIFO1	= 6,
	AXIS_SWITCH_DMA_SIMPLE	= 7,
	AXI_SWITCH_NUM		= 8 /* Number of master and slave interfaces */
};
#else
  #define HAS_DMA_SIMPLE	1
  #define HAS_SWITCH		1
  #define HAS_XSG		1

/** Base addresses in AXI bus of FPGA */
enum {
	BASEADDR_SWITCH		= 0x0000, /* AXI Stream switch registers */
	BASEADDR_DMA_SIMPLE	= 0x1000, /* Simple DMA controller registers */
	BASEADDR_RESET		= 0x2000, /* Internal Reset register */
	BASEADDR_RTDS		= 0x3000, /* RTDS to AXI Stream bridge */
	BASEADDR_XSG		= 0x4000, /* Xilinx System Generator Model */
	BASEADDR_INTC		= 0x5000, /* PCIe MSI Interrupt controller registers */
	BASEADDR_PCIE		= 0x10000000, /* PCIe config space */
	BASEADDR_HOST		= 0x80000000 /* Start of host memory (0x0 of IOMMU) */
};

/** MSI vector table for VILLASfpga components */
enum {
	MSI_DMA_MM2S		= 0, /* Sensivity: level high,  synchronous: axi_clk */
	MSI_DMA_S2MM		= 1, /* Sensivity: level high,  synchronous: axi_clk */
	MSI_RTDS_TS		= 2, /* Sensivity: rising edge, synchronous: rtds_clk100m (New timestep started) */
	MSI_RTDS_OVF		= 3, /* Sensivity: rising edge, synchronous: rtds_clk100m (UserTxInProgress overlapped with UserTStepPulse)*/
	MSI_RTDS_CASE		= 4  /* Sensivity: rising edge, synchronous: rtds_clk100m (New RSCAD case started) */
};

/** Mapping of VILLASfpga components to AXI Stream interconnect ports */
enum {
	AXIS_SWITCH_RTDS	= 0,
	AXIS_SWITCH_XSG		= 1,
	AXIS_SWITCH_DMA_SIMPLE	= 2,
	AXI_SWITCH_NUM		= 3 /* Number of master and slave interfaces */
};
#endif

#define PCI_VID_XILINX		0x10ee
#define PCI_PID_VFPGA		0x7022

#endif /* _CONFIG_FPGA_H_ */