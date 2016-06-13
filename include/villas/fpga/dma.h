/** DMA related helper functions
 *
 * These functions present a simpler interface to Xilinx' DMA driver (XAxiDma_*)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#ifndef _DMA_H_
#define _DMA_H_
 
#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>

#include <xilinx/xaxidma.h>

#define XAXIDMA_SR_SGINCL_MASK	0x00000008

enum dma_mode {
	DMA_MODE_SIMPLE,
	DMA_MODE_SG
};

struct dma_mem {
	uint32_t base_virt;
	uint32_t base_phys;
	size_t len;
};

int dma_init(XAxiDma *xdma, char *baseaddr, enum dma_mode mode, struct dma_mem *bd, struct dma_mem *rx, size_t maxlen);

ssize_t dma_write(XAxiDma *xdma, char *buf, size_t len, int irq);

ssize_t dma_read(XAxiDma *xdma, char *buf, size_t len, int irq);

ssize_t dma_ping_pong(XAxiDma *xdma, char *src, char *dst, size_t len, int s2mm_irq);

#endif /* _DMA_H_ */