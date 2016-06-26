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

struct dma {
	XAxiDma inst;
};

struct dma_mem {
	uint32_t base_virt;
	uint32_t base_phys;
	size_t len;
};

struct ip;

int dma_write(struct ip *c, char *buf, size_t len);
int dma_read(struct ip *c, char *buf, size_t len);
int dma_read_complete(struct ip *c, char **buf, size_t *len);
int dma_write_complete(struct ip *c, char **buf, size_t *len);

int dma_sg_write(struct ip *c, char *buf, size_t len);
int dma_sg_read(struct ip *c, char *buf, size_t len);

int dma_sg_write_complete(struct ip *c, char **buf, size_t *len);
int dma_sg_read_complete(struct ip *c, char **buf, size_t *len);

int dma_simple_read(struct ip *c, char *buf, size_t len);
int dma_simple_write(struct ip *c, char *buf, size_t len);

int dma_simple_read_complete(struct ip *c, char **buf, size_t *len);
int dma_simple_write_complete(struct ip *c, char **buf, size_t *len);

int dma_ping_pong(struct ip *c, char *src, char *dst, size_t len);

int dma_init(struct ip *c);
int dma_init_rings(struct ip *c, struct dma_mem *bd);

#endif /* _DMA_H_ */