/** DMA related helper functions
 *
 * These functions present a simpler interface to Xilinx' DMA driver (XAxiDma_*)
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASfpga. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

#include <villas/log.h>

#include "utils.h"
#include "fpga/dma.h"
#include "fpga/ip.h"

int dma_mem_split(struct dma_mem *o, struct dma_mem *a, struct dma_mem *b)
{
	int split = o->len / 2;

	a->base_virt = o->base_virt;
	a->base_phys = o->base_phys;

	b->base_virt = a->base_virt + split;
	b->base_phys = a->base_phys + split;

	a->len = split;
	b->len = o->len - split;

	return 0;
}

int dma_alloc(struct ip *c, struct dma_mem *mem, size_t len, int flags)
{
	int ret;

	/* Align to next bigger page size chunk */
	if (len & 0xFFF) {
		len += 0x1000;
		len &= ~0xFFF;
	}

	mem->len = len;
	mem->base_phys = -1; /* find free */
	mem->base_virt = mmap(0, mem->len, PROT_READ | PROT_WRITE, flags | MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, 0, 0);
	if (mem->base_virt == MAP_FAILED)
		return -1;

	ret = vfio_map_dma(c->card->vd.group->container, mem);
	if (ret)
		return -2;

	return 0;
}

int dma_free(struct ip *c, struct dma_mem *mem)
{
	int ret;
	
	ret = vfio_unmap_dma(c->card->vd.group->container, mem);
	if (ret)
		return ret;
	
	ret = munmap(mem->base_virt, mem->len);
	if (ret)
		return ret;
	
	return 0;
}

int dma_ping_pong(struct ip *c, char *src, char *dst, size_t len)
{
	int ret;

	ret = dma_read(c, dst, len);
	if (ret)
		return ret;
	
	ret = dma_write(c, src, len);
	if (ret)
		return ret;
	
	ret = dma_write_complete(c, NULL, NULL);
	if (ret)
		return ret;
	
	ret = dma_read_complete(c, NULL, NULL);
	if (ret)
		return ret;

	return 0;
}

int dma_write(struct ip *c, char *buf, size_t len)
{
	XAxiDma *xdma = &c->dma.inst;

	debug(25, "DMA write: dmac=%s buf=%p len=%#x", c->name, buf, len);

	return xdma->HasSg
		? dma_sg_write(c, buf, len)
		: dma_simple_write(c, buf, len);
}

int dma_read(struct ip *c, char *buf, size_t len)
{
	XAxiDma *xdma = &c->dma.inst;
	
	debug(25, "DMA read: dmac=%s buf=%p len=%#x", c->name, buf, len);

	return xdma->HasSg
		? dma_sg_read(c, buf, len)
		: dma_simple_read(c, buf, len);
}

int dma_read_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma.inst;
	
	debug(25, "DMA read complete: dmac=%s", c->name);

	return xdma->HasSg
		? dma_sg_read_complete(c, buf, len)
		: dma_simple_read_complete(c, buf, len);
}

int dma_write_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma.inst;
	
	debug(25, "DMA write complete: dmac=%s", c->name);

	return xdma->HasSg
		? dma_sg_write_complete(c, buf, len)
		: dma_simple_write_complete(c, buf, len);
}

int dma_sg_write(struct ip *c, char *buf, size_t len)
{
	int ret, bdcnt;

	XAxiDma *xdma = &c->dma.inst;
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(xdma);
	XAxiDma_Bd *bds, *bd;

	uint32_t remaining, bdlen, bdbuf, cr;

	/* Checks */
	if (!xdma->HasSg)
		return -1;

	if (len < 1)
		return -2;

	if (!xdma->HasMm2S)
		return -3;

	if (!ring->HasDRE) {
		uint32_t mask = xdma->MicroDmaMode ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN : ring->DataWidth - 1;
		if ((uintptr_t) buf & mask)
			return -4;
	}

	bdcnt = CEIL(len, FPGA_DMA_BOUNDARY);
	ret = XAxiDma_BdRingAlloc(ring, bdcnt, &bds);
	if (ret != XST_SUCCESS)
		return -5;

	remaining = len;
	bdbuf = (uint32_t) buf;
	bd = bds;
	for (int i = 0; i < bdcnt; i++) {
		bdlen = MIN(remaining, FPGA_DMA_BOUNDARY);

		ret = XAxiDma_BdSetBufAddr(bd, bdbuf);
		if (ret != XST_SUCCESS)
			goto out;

		ret = XAxiDma_BdSetLength(bd, bdlen, ring->MaxTransferLen);
		if (ret != XST_SUCCESS)
			goto out;

		/* Set SOF / EOF / ID */
		cr = 0;
		if (i == 0)
			cr |= XAXIDMA_BD_CTRL_TXSOF_MASK;
		if (i == bdcnt - 1)
			cr |= XAXIDMA_BD_CTRL_TXEOF_MASK;
	
		XAxiDma_BdSetCtrl(bd, cr);
		XAxiDma_BdSetId(bd, (uintptr_t) buf);

		remaining -= bdlen;
		bdbuf += bdlen;
		bd = (XAxiDma_Bd *) XAxiDma_BdRingNext(ring, bd);
	}

	/* Give the BD to DMA to kick off the transmission. */
	ret = XAxiDma_BdRingToHw(ring, bdcnt, bds);
	if (ret != XST_SUCCESS)
		return -8;

	return 0;

out:
	ret = XAxiDma_BdRingUnAlloc(ring, bdcnt, bds);
	if (ret != XST_SUCCESS)
		return -6;

	return -5;
}

int dma_sg_read(struct ip *c, char *buf, size_t len)
{
	int ret, bdcnt;

	XAxiDma *xdma = &c->dma.inst;
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(xdma);
	XAxiDma_Bd *bds, *bd;

	uint32_t remaining, bdlen, bdbuf;

	/* Checks */
	if (!xdma->HasSg)
		return -1;

	if (len < 1)
		return -2;

	if (!xdma->HasS2Mm)
		return -3;

	if (!ring->HasDRE) {
		uint32_t mask = xdma->MicroDmaMode ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN : ring->DataWidth - 1;
		if ((uintptr_t) buf & mask)
			return -4;
	}

	bdcnt = CEIL(len, FPGA_DMA_BOUNDARY);
	ret = XAxiDma_BdRingAlloc(ring, bdcnt, &bds);
	if (ret != XST_SUCCESS)
		return -5;

	bdbuf = (uint32_t) buf;
	remaining = len;
	bd = bds;
	for (int i = 0; i < bdcnt; i++) {
		bdlen = MIN(remaining, FPGA_DMA_BOUNDARY);
		ret = XAxiDma_BdSetLength(bd, bdlen, ring->MaxTransferLen);
		if (ret != XST_SUCCESS)
			goto out;

		ret = XAxiDma_BdSetBufAddr(bd, bdbuf);
		if (ret != XST_SUCCESS)
			goto out;

		/* Receive BDs do not need to set anything for the control
		* The hardware will set the SOF/EOF bits per stream ret */
		XAxiDma_BdSetCtrl(bd, 0);
		XAxiDma_BdSetId(bd, (uintptr_t) buf);

		remaining -= bdlen;
		bdbuf += bdlen;
		bd = (XAxiDma_Bd *) XAxiDma_BdRingNext(ring, bd);
	}

	ret = XAxiDma_BdRingToHw(ring, bdcnt, bds);
	if (ret != XST_SUCCESS)
		return -8;

	return 0;

out:
	ret = XAxiDma_BdRingUnAlloc(ring, bdcnt, bds);
	if (ret != XST_SUCCESS)
		return -6;

	return -5;
}

int dma_sg_write_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma.inst;
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(xdma);
	XAxiDma_Bd *bds;

	int processed, ret;

	/* Wait until the one BD TX transaction is done */
	while (!(XAxiDma_IntrGetIrq(xdma, XAXIDMA_DMA_TO_DEVICE) & XAXIDMA_IRQ_IOC_MASK))
		intc_wait(c->card->intc, c->irq);
	XAxiDma_IntrAckIrq(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
	
	processed = XAxiDma_BdRingFromHw(ring, XAXIDMA_ALL_BDS, &bds);
	
	if (len != NULL)
		*len = XAxiDma_BdGetActualLength(bds, XAXIDMA_MAX_TRANSFER_LEN);

	if (buf != NULL)
		*buf = (char *) (uintptr_t) XAxiDma_BdGetId(bds);

	/* Free all processed TX BDs for future transmission */
	ret = XAxiDma_BdRingFree(ring, processed, bds);
	if (ret != XST_SUCCESS)
		return -1;

	return 0;
}

int dma_sg_read_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma.inst;
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(xdma);
	XAxiDma_Bd *bds, *bd;

	int ret, bdcnt;
	uint32_t recvlen, sr;
	uintptr_t recvbuf;

	if (!xdma->HasSg)
		return -1;

	while (!(XAxiDma_IntrGetIrq(xdma, XAXIDMA_DEVICE_TO_DMA) & XAXIDMA_IRQ_IOC_MASK))
		intc_wait(c->card->intc, c->irq + 1);
	XAxiDma_IntrAckIrq(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	bdcnt = XAxiDma_BdRingFromHw(ring, XAXIDMA_ALL_BDS, &bds);

	recvlen = 0;

	bd = bds;
	for (int i = 0; i < bdcnt; i++) {
		recvlen += XAxiDma_BdGetActualLength(bd, ring->MaxTransferLen);
		
		sr = XAxiDma_BdGetSts(bd);
		if (sr & XAXIDMA_BD_STS_RXSOF_MASK)
			if (i != 0)
				warn("sof not first");
		
		if (sr & XAXIDMA_BD_STS_RXEOF_MASK)
			if (i != bdcnt - 1)
				warn("eof not last");

		recvbuf = XAxiDma_BdGetId(bd);

 		bd = (XAxiDma_Bd *) XAxiDma_BdRingNext(ring, bd);
	}
	
	if (len != NULL)
		*len = recvlen;
	if (buf != NULL)
		*buf = (char *) recvbuf;

	/* Free all processed RX BDs for future transmission */
	ret = XAxiDma_BdRingFree(ring, bdcnt, bds);
	if (ret != XST_SUCCESS)
		return -3;
	
	return 0;
}

int dma_simple_read(struct ip *c, char *buf, size_t len)
{
	XAxiDma *xdma = &c->dma.inst;
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(xdma);

	/* Checks */
	if (xdma->HasSg)
		return -1;

	if ((len < 1) || (len > FPGA_DMA_BOUNDARY))
		return -2;

	if (!xdma->HasS2Mm)
		return -3;
	
	if (!ring->HasDRE) {
		uint32_t mask = xdma->MicroDmaMode ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN : ring->DataWidth - 1;
		if ((uintptr_t) buf & mask)
			return -4;
	}
	
	if(!(XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK)) {
		if (XAxiDma_Busy(xdma, XAXIDMA_DEVICE_TO_DMA))
			return -5;
	}

	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_DESTADDR_OFFSET, LOWER_32_BITS((uintptr_t) buf));
	if (xdma->AddrWidth > 32)
		XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_DESTADDR_MSB_OFFSET,  UPPER_32_BITS((uintptr_t) buf));

	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_CR_OFFSET, XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_CR_OFFSET) | XAXIDMA_CR_RUNSTOP_MASK);
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET, len);

	return XST_SUCCESS;
}

int dma_simple_write(struct ip *c, char *buf, size_t len)
{
	XAxiDma *xdma = &c->dma.inst;
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(xdma);

	/* Checks */
	if (xdma->HasSg)
		return -1;

	if ((len < 1) || (len > FPGA_DMA_BOUNDARY))
		return -2;

	if (!xdma->HasMm2S)
		return -3;

	if (!ring->HasDRE) {
		uint32_t mask = xdma->MicroDmaMode ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN : ring->DataWidth - 1;
		if ((uintptr_t) buf & mask)
			return -4;
	}

	/* If the engine is doing transfer, cannot submit  */
	if(!(XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK)) {
		if (XAxiDma_Busy(xdma, XAXIDMA_DMA_TO_DEVICE))
			return -5;
	}

	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_SRCADDR_OFFSET, LOWER_32_BITS((uintptr_t) buf));
	if (xdma->AddrWidth > 32)
		XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_SRCADDR_MSB_OFFSET, UPPER_32_BITS((uintptr_t) buf));

	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_CR_OFFSET, XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_CR_OFFSET) | XAXIDMA_CR_RUNSTOP_MASK);
	XAxiDma_WriteReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET, len);

	return XST_SUCCESS;
}

int dma_simple_read_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma.inst;
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(xdma);

	while (!(XAxiDma_IntrGetIrq(xdma, XAXIDMA_DEVICE_TO_DMA) & XAXIDMA_IRQ_IOC_MASK))
		intc_wait(c->card->intc, c->irq + 1);
	XAxiDma_IntrAckIrq(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	if (len)
		*len = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET);

	if (buf) {
		*buf = (char *) (uintptr_t) XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_DESTADDR_OFFSET);
		if (xdma->AddrWidth > 32)
			*buf += XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_DESTADDR_MSB_OFFSET);
	}

	return 0;
}

int dma_simple_write_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma.inst;
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(xdma);

	while (!(XAxiDma_IntrGetIrq(xdma, XAXIDMA_DMA_TO_DEVICE) & XAXIDMA_IRQ_IOC_MASK))
		intc_wait(c->card->intc, c->irq);
	XAxiDma_IntrAckIrq(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);

	if (len)
		*len = XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_BUFFLEN_OFFSET);

	if (buf) {
		*buf = (char *) (uintptr_t) XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SRCADDR_OFFSET);
		if (xdma->AddrWidth > 32)
			*buf += XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SRCADDR_MSB_OFFSET);
	}

	return 0;
}

static int dma_setup_ring(XAxiDma_BdRing *ring, struct dma_mem *bdbuf)
{
	int delay = 0;
	int coalesce = 1;
	int ret, cnt;

	XAxiDma_Bd clearbd;

	/* Disable all RX interrupts before RxBD space setup */
	XAxiDma_BdRingIntDisable(ring, XAXIDMA_IRQ_ALL_MASK);

	/* Set delay and coalescing */
	XAxiDma_BdRingSetCoalesce(ring, coalesce, delay);

	/* Setup Rx BD space */
	cnt = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT, bdbuf->len);

	ret = XAxiDma_BdRingCreate(ring, (uintptr_t) bdbuf->base_phys, (uintptr_t) bdbuf->base_virt, XAXIDMA_BD_MINIMUM_ALIGNMENT, cnt);
	if (ret != XST_SUCCESS)
		return -1;

	XAxiDma_BdClear(&clearbd);
	ret = XAxiDma_BdRingClone(ring, &clearbd);
	if (ret != XST_SUCCESS)
		return -2;

	/* Start the channel */
	ret = XAxiDma_BdRingStart(ring);
	if (ret != XST_SUCCESS)
		return -3;

	return XST_SUCCESS;
}

static int dma_init_rings(XAxiDma *xdma, struct dma_mem *bd)
{
	int ret;

	struct dma_mem bd_rx, bd_tx;

	ret = dma_mem_split(bd, &bd_rx, &bd_tx);
	if (ret)
		return -1;

	ret = dma_setup_ring(XAxiDma_GetRxRing(xdma), &bd_rx);
	if (ret != XST_SUCCESS)
		return -2;

	ret = dma_setup_ring(XAxiDma_GetTxRing(xdma), &bd_tx);
	if (ret != XST_SUCCESS)
		return -3;

	return 0;
}

int dma_init(struct ip *c)
{
	int ret, sg;
	struct dma *dma = &c->dma;
	XAxiDma *xdma = &dma->inst;
	
	/* Guess DMA type */
	sg = (XAxiDma_In32((uintptr_t) c->card->map + c->baseaddr + XAXIDMA_TX_OFFSET+ XAXIDMA_SR_OFFSET) &
	      XAxiDma_In32((uintptr_t) c->card->map + c->baseaddr + XAXIDMA_RX_OFFSET+ XAXIDMA_SR_OFFSET) & XAXIDMA_SR_SGINCL_MASK) ? 1 : 0;

	XAxiDma_Config xdma_cfg = {
		.BaseAddr = (uintptr_t) c->card->map + c->baseaddr,
		.HasStsCntrlStrm = 0,
		.HasMm2S = 1,
		.HasMm2SDRE = 1,
		.Mm2SDataWidth = 128,
		.HasS2Mm = 1,
		.HasS2MmDRE = 1, /* Data Realignment Engine */
		.HasSg = sg,
		.S2MmDataWidth = 128,
		.Mm2sNumChannels = 1,
		.S2MmNumChannels = 1,
		.Mm2SBurstSize = 64,
		.S2MmBurstSize = 64,
		.MicroDmaMode = 0,
		.AddrWidth = 32
	};
	
	ret = XAxiDma_CfgInitialize(xdma, &xdma_cfg);
	if (ret != XST_SUCCESS)
		return -1;

	/* Perform selftest */
	ret = XAxiDma_Selftest(xdma);
	if (ret != XST_SUCCESS)
		return -2;
	
	/* Map buffer descriptors */
	if (xdma->HasSg) {
		ret = dma_alloc(c, &dma->bd, FPGA_DMA_BD_SIZE, 0);
		if (ret)
			return -3;

		ret = dma_init_rings(xdma, &dma->bd);
		if (ret)
			return -4;
	}

	/* Enable completion interrupts for both channels */
	XAxiDma_IntrEnable(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrEnable(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	return 0;
}

int dma_reset(struct ip *c)
{
	XAxiDma_Reset(&c->dma.inst);

	return 0;
}

static struct ip_type ip = {
	.vlnv = { "xilinx.com", "ip", "axi_dma", NULL },
	.init = dma_init,
	.reset = dma_reset
};

REGISTER_IP_TYPE(&ip)