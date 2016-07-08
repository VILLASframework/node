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

#define FPGA_DMA_BASEADDR	0x00000000
#define FPGA_DMA_BOUNDARY	0x1000
#define FPGA_DMA_BD_OFFSET	0xC0000000
#define FPGA_DMA_BD_SIZE	(32 << 20) // 32 MB

#define XAXIDMA_SR_SGINCL_MASK	0x00000008

struct dma_mem {
	char *base_virt;
	char *base_phys;
	size_t len;
};

struct dma {
	XAxiDma inst;

	struct dma_mem bd;
};

struct ip;

int dma_mem_split(struct dma_mem *o, struct dma_mem *a, struct dma_mem *b);

int dma_alloc(struct ip *c, struct dma_mem *mem, size_t len, int flags);
int dma_free(struct ip *c, struct dma_mem *mem);

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