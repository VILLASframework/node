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

#include <villas/log.h>

#include "utils.h"
#include "fpga/dma.h"
#include "fpga/ip.h"

int dma_ping_pong(struct ip *c, char *src, char *dst, size_t len)
{
	int ret;

	ret = dma_read(c, dst, len);
	if (ret)
		return ret;
	
	ret = dma_write(c, src, len);
	if (ret)
		return ret;
	
	ret = dma_read_complete(c, NULL, NULL);
	if (ret)
		return ret;

	ret = dma_write_complete(c, NULL, NULL);
	if (ret)
		return ret;
	
	return 0;
}

int dma_write(struct ip *c, char *buf, size_t len)
{
	XAxiDma *xdma = &c->dma;

	return xdma->HasSg
		? dma_sg_write(c, buf, len)
		: dma_simple_write(c, buf, len);
}

int dma_read(struct ip *c, char *buf, size_t len)
{
	XAxiDma *xdma = &c->dma;

	return xdma->HasSg
		? dma_sg_read(c, buf, len)
		: dma_simple_read(c, buf, len);
}

int dma_read_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma;

	return xdma->HasSg
		? dma_sg_read_complete(c, buf, len)
		: dma_simple_read_complete(c, buf, len);
}

int dma_write_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma;

	return xdma->HasSg
		? dma_sg_write_complete(c, buf, len)
		: dma_simple_write_complete(c, buf, len);
}

int dma_sg_write(struct ip *c, char *buf, size_t len)
{
	XAxiDma *xdma = &c->dma;
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(xdma);
	XAxiDma_Bd *bd;

	int ret;
	
	buf = virt_to_dma(buf, c->card->dma);

	/* Checks */
	if (!xdma->HasSg)
		return -1;

	if ((len < 1) || (len > ring->MaxTransferLen))
		return -2;

	if (!xdma->HasMm2S)
		return -3;

	if (!ring->HasDRE) {
		uint32_t mask = xdma->MicroDmaMode ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN : ring->DataWidth - 1;
		if ((uintptr_t) buf & mask)
			return -4;
	}

	ret = XAxiDma_BdRingAlloc(ring, 1, &bd);
	if (ret != XST_SUCCESS)
		return -5;

	/* Set up the BD using the information of the packet to transmit */
	ret = XAxiDma_BdSetBufAddr(bd, (uintptr_t) buf);
	if (ret != XST_SUCCESS)
		return -6;

	ret = XAxiDma_BdSetLength(bd, len, ring->MaxTransferLen);
	if (ret != XST_SUCCESS)
		return -7;

	/* Set SOF / EOF / ID */
	XAxiDma_BdSetCtrl(bd, XAXIDMA_BD_CTRL_TXEOF_MASK | XAXIDMA_BD_CTRL_TXSOF_MASK);
	XAxiDma_BdSetId(bd, (uintptr_t) buf);

	/* Give the BD to DMA to kick off the transmission. */
	ret = XAxiDma_BdRingToHw(ring, 1, bd);
	if (ret != XST_SUCCESS)
		return -8;

	return 0;
}

int dma_sg_read(struct ip *c, char *buf, size_t len)
{
	XAxiDma *xdma = &c->dma;
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(xdma);
	XAxiDma_Bd *bd;

	int ret;
	
	buf = virt_to_dma(buf, c->card->dma);


	/* Checks */
	if (!xdma->HasSg)
		return -1;

	if ((len < 1) || (len > ring->MaxTransferLen))
		return -2;

	if (!xdma->HasS2Mm)
		return -3;

	if (!ring->HasDRE) {
		uint32_t mask = xdma->MicroDmaMode ? XAXIDMA_MICROMODE_MIN_BUF_ALIGN : ring->DataWidth - 1;
		if ((uintptr_t) buf & mask)
			return -4;
	}

	ret = XAxiDma_BdRingAlloc(ring, 1, &bd);
	if (ret != XST_SUCCESS)
		return -5;

	ret = XAxiDma_BdSetBufAddr(bd, (uintptr_t) buf);
	if (ret != XST_SUCCESS)
		return -6;

	ret = XAxiDma_BdSetLength(bd, len, ring->MaxTransferLen);
	if (ret != XST_SUCCESS)
		return -7;

	/* Receive BDs do not need to set anything for the control
	 * The hardware will set the SOF/EOF bits per stream ret */
	XAxiDma_BdSetCtrl(bd, 0);
	XAxiDma_BdSetId(bd, (uintptr_t) buf);

	ret = XAxiDma_BdRingToHw(ring, 1, bd);
	if (ret != XST_SUCCESS)
		return -8;

	return 0;
}

int dma_sg_write_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma;
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(xdma);
	XAxiDma_Bd *bd;

	int processed, ret;

	
	/* Wait until the one BD TX transaction is done */
	while ((processed = XAxiDma_BdRingFromHw(ring, XAXIDMA_ALL_BDS, &bd)) == 0) {
//		if (c->irq >= 0) {
//			intc_wait(c->card, c->irq);
//			XAxiDma_IntrAckIrq(xmda, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
//		}
//		else
			__asm__ ("nop");
	}

	/* Free all processed TX BDs for future transmission */
	ret = XAxiDma_BdRingFree(ring, processed, bd);
	if (ret != XST_SUCCESS) {
		info("Failed to free %d tx BDs %d", processed, ret);
		return -1;
	}
	
	return 0;
}

int dma_sg_read_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma;
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(xdma);
	XAxiDma_Bd *bd;

	int ret;

	if (!xdma->HasSg)
		return -1;

	while (XAxiDma_BdRingFromHw(ring, 1, &bd) == 0) {
//		if (irq >= 0) {
//			intc_wait(c->card, c->irq + 1);
//			XAxiDma_IntrAckIrq(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);
//		}
//		else
			__asm__("nop");
	}

	if (len != NULL)
		*len = XAxiDma_BdGetActualLength(bd, XAXIDMA_MAX_TRANSFER_LEN);
	
	if (buf != NULL)
		*buf = (char *) (uintptr_t) XAxiDma_BdGetBufAddr(bd);

	/* Free all processed RX BDs for future transmission */
	ret = XAxiDma_BdRingFree(ring, 1, bd);
	if (ret != XST_SUCCESS)
		return -3;
	
	return 0;
}

int dma_simple_read(struct ip *c, char *buf, size_t len)
{
	XAxiDma *xdma = &c->dma;
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(xdma);

	buf = virt_to_dma(buf, c->card->dma);

	/* Checks */
	if (xdma->HasSg)
		return -1;

	if ((len < 1) || (len > ring->MaxTransferLen))
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
	XAxiDma *xdma = &c->dma;
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(xdma);

	buf = virt_to_dma(buf, c->card->dma);

	/* Checks */
	if (xdma->HasSg)
		return -1;

	if ((len < 1) || (len > ring->MaxTransferLen))
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
	XAxiDma *xdma = &c->dma;
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(xdma);

	while (!(XAxiDma_IntrGetIrq(xdma, XAXIDMA_DEVICE_TO_DMA) & XAXIDMA_IRQ_IOC_MASK));
	
//	if(!(XAxiDma_ReadReg(ring->ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK)) {
//		if (XAxiDma_Busy(xdma, XAXIDMA_DMA_TO_DEVICE))
//			return -5;
//	}

	return 0;
}

int dma_simple_write_complete(struct ip *c, char **buf, size_t *len)
{
	XAxiDma *xdma = &c->dma;
	XAxiDma_BdRing *ring = XAxiDma_GetRxRing(xdma);

	while (!(XAxiDma_IntrGetIrq(xdma, XAXIDMA_DEVICE_TO_DMA) & XAXIDMA_IRQ_IOC_MASK));

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

	ret = XAxiDma_BdRingCreate(ring, bdbuf->base_phys, bdbuf->base_virt, XAXIDMA_BD_MINIMUM_ALIGNMENT, cnt);
	if (ret != XST_SUCCESS) {
		info("RX create BD ring failed %d", ret);
		return XST_FAILURE;
	}

	XAxiDma_BdClear(&clearbd);
	ret = XAxiDma_BdRingClone(ring, &clearbd);
	if (ret != XST_SUCCESS) {
		info("RX clone BD failed %d", ret);
		return XST_FAILURE;
	}
	
	/* Start the channel */
	ret = XAxiDma_BdRingStart(ring);
	if (ret != XST_SUCCESS) {
		info("failed start bdring txsetup %d", ret);
		return XST_FAILURE;
	}
	
	return XST_SUCCESS;
}

int dma_init_rings(struct ip *c, struct dma_mem *bd)
{
	int ret;
	XAxiDma *xdma = &c->dma;


	/* Split BD memory equally between Rx and Tx rings */
	struct dma_mem bd_rx = {
		.base_virt = bd->base_virt,
		.base_phys = bd->base_phys,
		.len = bd->len / 2
	};
	struct dma_mem bd_tx = {
		.base_virt = bd->base_virt + bd_rx.len,
		.base_phys = bd->base_phys + bd_rx.len,
		.len = bd->len - bd_rx.len
	};

	ret = dma_setup_ring(XAxiDma_GetRxRing(xdma), &bd_rx);
	if (ret != XST_SUCCESS)
		return -1;

	ret = dma_setup_ring(XAxiDma_GetTxRing(xdma), &bd_tx);
	if (ret != XST_SUCCESS)
		return -1;

	return 0;
}

int dma_init(struct ip *c)
{
	int ret, sg;
	XAxiDma *xdma = &c->dma;
	
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
	if (ret != XST_SUCCESS) {
		info("Initialization failed %d", ret);
		return -1;
	}

	/* Perform selftest */
	ret = XAxiDma_Selftest(xdma);
	if (ret != XST_SUCCESS) {
		info("DMA selftest failed");
		return -1;
	}

	/* Enable completion interrupts for both channels */
	XAxiDma_IntrEnable(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrEnable(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	return 0;
}