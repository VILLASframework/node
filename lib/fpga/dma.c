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

int dma_write_complete(XAxiDma *xdma, int irq)
{
	int processed, ret;

	XAxiDma_Bd *bd;
	XAxiDma_BdRing *ring = XAxiDma_GetTxRing(xdma);

	/* Wait until the one BD TX transaction is done */
	while ((processed = XAxiDma_BdRingFromHw(ring, XAXIDMA_ALL_BDS, &bd)) == 0) {
		if (irq >= 0)
			wait_irq(irq);
	}

	/* Free all processed TX BDs for future transmission */
	ret = XAxiDma_BdRingFree(ring, processed, bd);
	if (ret != XST_SUCCESS) {
		info("Failed to free %d tx BDs %d", processed, ret);
		return -1;
	}
	
	return processed;
}

int dma_read_complete(XAxiDma *xdma, int irq)
{
	return 0;
}

ssize_t dma_write(XAxiDma *xdma, char *buf, size_t len, int irq)
{
	int ret;
	
	if (xdma->HasSg) {

		XAxiDma_BdRing *ring;
		XAxiDma_Bd *bd;

		ring = XAxiDma_GetTxRing(xdma);

		ret = XAxiDma_BdRingAlloc(ring, 1, &bd);
		if (ret != XST_SUCCESS)
			return -1;

		/* Set up the BD using the information of the packet to transmit */
		ret = XAxiDma_BdSetBufAddr(bd, (uintptr_t) buf);
		if (ret != XST_SUCCESS) {
			info("Tx set buffer addr %p on BD %p failed %d", buf, bd, ret);
			return -1;
		}

		ret = XAxiDma_BdSetLength(bd, len, ring->MaxTransferLen);
		if (ret != XST_SUCCESS) {
			info("Tx set length %zu on BD %p failed %d", len, bd, ret);
			return -1;
		}

		/* Set SOF / EOF / ID */
		XAxiDma_BdSetCtrl(bd, XAXIDMA_BD_CTRL_TXEOF_MASK | XAXIDMA_BD_CTRL_TXSOF_MASK);
		XAxiDma_BdSetId(bd, (uintptr_t) buf);

		/* Give the BD to DMA to kick off the transmission. */
		ret = XAxiDma_BdRingToHw(ring, 1, bd);
		if (ret != XST_SUCCESS) {
			info("to hw failed %d", ret);
			return -1;
		}
		
		dma_write_complete(xdma, irq);
	}
	else {
		ret = XAxiDma_SimpleTransfer(xdma, (uintptr_t) buf, len, XAXIDMA_DMA_TO_DEVICE);
		if (ret != XST_SUCCESS) {
			warn("Failed DMA transfer");
			return -1;
		}

		if (irq >= 0) {
			wait_irq(irq);
			XAxiDma_IntrAckIrq(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
		}
		else {
			while (XAxiDma_Busy(xdma, XAXIDMA_DMA_TO_DEVICE))
				__asm__("nop");
		}
	}

	return len;
}

ssize_t dma_read(XAxiDma *xdma, char *buf, size_t len, int irq)
{
	int ret;
	
	if (xdma->HasSg) {
		int freecnt, processed;

		XAxiDma_BdRing *ring;
		XAxiDma_Bd *bd;

		ring = XAxiDma_GetRxRing(xdma);

		processed = 0;
		do {
			/* Wait until the data has been received by the Rx channel */
			while (XAxiDma_BdRingFromHw(ring, 1, &bd) == 0) {
				if (irq >= 0)
					wait_irq(irq);
			}
			
			//char *bdbuf = XAxiDma_BdGetBufAddr(bd);
			//size_t bdlen = XAxiDma_BdGetActualLength(bd, XAXIDMA_MAX_TRANSFER_LEN);

			//memcpy(buf, bdbuf, bdlen);
			//buf += bdlen;

			processed++;
		} while (XAxiDma_BdGetSts(bd) & XAXIDMA_BD_STS_RXEOF_MASK);

		/* Free all processed RX BDs for future transmission */
		ret = XAxiDma_BdRingFree(ring, processed, bd);
		if (ret != XST_SUCCESS) {
			info("Failed to free %d rx BDs %d", processed, ret);
			return XST_FAILURE;
		}

		/* Return processed BDs to RX channel so we are ready to receive new
		 * packets:
		 *    - Allocate all free RX BDs
		 *    - Pass the BDs to RX channel
		 */
		freecnt = XAxiDma_BdRingGetFreeCnt(ring);
		ret = XAxiDma_BdRingAlloc(ring, freecnt, &bd);
		if (ret != XST_SUCCESS) {
			info("bd alloc failed");
			return -1;
		}

		ret = XAxiDma_BdRingToHw(ring, freecnt, bd);
		if (ret != XST_SUCCESS) {
			info("Submit %d rx BDs failed %d", freecnt, ret);
			return -1;
		}
	}
	else {
		ret = XAxiDma_SimpleTransfer(xdma, (uintptr_t) buf, len, XAXIDMA_DEVICE_TO_DMA);
		if (ret != XST_SUCCESS)
			warn("Failed DMA transfer");

		if (irq >= 0) {
			wait_irq(irq);
			XAxiDma_IntrAckIrq(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);
		}
		else {
			while (XAxiDma_Busy(xdma, XAXIDMA_DEVICE_TO_DMA))
				__asm__("nop");
		}
	}

	return len;
}

ssize_t dma_ping_pong(XAxiDma *xdma, char *src, char *dst, size_t len, int s2mm_irq)
{
	int ret;

	/* Prepare S2MM transfer */
	ret = XAxiDma_SimpleTransfer(xdma, (uintptr_t) dst, len, XAXIDMA_DEVICE_TO_DMA);
	if (ret != XST_SUCCESS)
		warn("Failed DMA transfer");

	/* Start MM2S transfer */
	ret = XAxiDma_SimpleTransfer(xdma, (uintptr_t) src, len, XAXIDMA_DMA_TO_DEVICE);
	if (ret != XST_SUCCESS) {
		warn("Failed DMA transfer");
		return -1;
	}

	/* Wait for completion */
	if (s2mm_irq >= 0) {
		wait_irq(s2mm_irq);
		XAxiDma_IntrAckIrq(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);
	}
	else {
		while (XAxiDma_Busy(xdma, XAXIDMA_DEVICE_TO_DMA))
			__asm__("nop");
	}
	
	return XAxiDma_ReadReg(xdma->RegBase + XAXIDMA_RX_OFFSET, XAXIDMA_BUFFLEN_OFFSET);
}

static int dma_setup_bdring(XAxiDma_BdRing *ring, struct dma_mem *bdbuf)
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

	/*
	 * Setup an all-zero BD as the template for the Rx channel.
	 */
	XAxiDma_BdClear(&clearbd);
	ret = XAxiDma_BdRingClone(ring, &clearbd);
	if (ret != XST_SUCCESS) {
		info("RX clone BD failed %d", ret);
		return XST_FAILURE;
	}
	
	return XST_SUCCESS;
}

static int dma_setup_rx(XAxiDma *AxiDmaInstPtr, struct dma_mem *bdbuf, struct dma_mem *rxbuf, size_t maxlen)
{
	int ret;
	
	XAxiDma_BdRing *ring;
	XAxiDma_Bd *bd, *curbd;

	uint32_t freecnt;
	uintptr_t rxptr;

	ring = XAxiDma_GetRxRing(AxiDmaInstPtr);
	ret = dma_setup_bdring(ring, bdbuf);
	if (ret != XST_SUCCESS)
		return -1;

	/* Attach buffers to RxBD ring so we are ready to receive packets */
	freecnt = XAxiDma_BdRingGetFreeCnt(ring);
	ret = XAxiDma_BdRingAlloc(ring, freecnt, &bd);
	if (ret != XST_SUCCESS) {
		info("RX alloc BD failed %d", ret);
		return XST_FAILURE;
	}

	curbd = bd;
	rxptr = rxbuf->base_phys;
	for (int i = 0; i < freecnt; i++) {
		ret = XAxiDma_BdSetBufAddr(curbd, rxptr);

		if (ret != XST_SUCCESS) {
			info("Set buffer addr %#lx on BD %p failed %d", rxptr, curbd, ret);
			return XST_FAILURE;
		}

		ret = XAxiDma_BdSetLength(curbd, maxlen, ring->MaxTransferLen);
		if (ret != XST_SUCCESS) {
			info("Rx set length %zu on BD %p failed %d", maxlen, curbd, ret);
			return XST_FAILURE;
		}

		/* Receive BDs do not need to set anything for the control
		 * The hardware will set the SOF/EOF bits per stream ret */
		XAxiDma_BdSetCtrl(curbd, 0);
		XAxiDma_BdSetId(curbd, rxptr);

		rxptr += maxlen;
		curbd = (XAxiDma_Bd *) XAxiDma_BdRingNext(ring, curbd);
	}

	/* Clear the receive buffer, so we can verify data */
	memset((void *) (uintptr_t) rxbuf->base_virt, 0, rxbuf->len);

	ret = XAxiDma_BdRingToHw(ring, freecnt, bd);
	if (ret != XST_SUCCESS) {
		info("RX submit hw failed %d", ret);
		return XST_FAILURE;
	}

	/* Start RX DMA channel */
	ret = XAxiDma_BdRingStart(ring);
	if (ret != XST_SUCCESS) {
		info("RX start hw failed %d", ret);

		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

static int dma_setup_tx(XAxiDma *AxiDmaInstPtr, struct dma_mem *bd)
{
	int ret;

	XAxiDma_BdRing *ring;

	ring = XAxiDma_GetTxRing(AxiDmaInstPtr);
	ret = dma_setup_bdring(ring, bd);
	if (ret != XST_SUCCESS)
		return -1;

	/* Start the TX channel */
	ret = XAxiDma_BdRingStart(ring);
	if (ret != XST_SUCCESS) {
		info("failed start bdring txsetup %d", ret);
		return XST_FAILURE;
	}
	

	return XST_SUCCESS;
}

int dma_init(XAxiDma *xdma, char *baseaddr, enum dma_mode mode, struct dma_mem *bd, struct dma_mem *rx, size_t maxlen)
{
	int ret, sgincld;

	XAxiDma_Config xdma_cfg = {
		.BaseAddr = (uintptr_t) baseaddr,
		.HasStsCntrlStrm = 0,
		.HasMm2S = 1,
		.HasMm2SDRE = 0,
		.Mm2SDataWidth = 32,
		.HasS2Mm = 1,
		.HasS2MmDRE = 0, /* Data Realignment Engine */
		.S2MmDataWidth = 32,
		.HasSg = (mode == DMA_MODE_SG) ? 1 : 0,
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

	/* Check if correct DMA type */
	sgincld = XAxiDma_In32((uintptr_t) baseaddr + XAXIDMA_TX_OFFSET+ XAXIDMA_SR_OFFSET) &
		  XAxiDma_In32((uintptr_t) baseaddr + XAXIDMA_RX_OFFSET+ XAXIDMA_SR_OFFSET) &
		  XAXIDMA_SR_SGINCL_MASK;

	if ((mode == DMA_MODE_SIMPLE && sgincld == XAXIDMA_SR_SGINCL_MASK) ||
	    (mode == DMA_MODE_SG     && sgincld != XAXIDMA_SR_SGINCL_MASK)) {
		info("This is not a %s DMA controller", (mode == DMA_MODE_SG) ? "SG" : "Simple");
		return -1;
	}

	/* Perform selftest */
	ret = XAxiDma_Selftest(xdma);
	if (ret != XST_SUCCESS) {
		info("DMA selftest failed");
		return -1;
	}

	/* Setup Buffer Descriptor rings for SG DMA */
	if (mode == DMA_MODE_SG) {
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

		ret = dma_setup_rx(xdma, &bd_rx, rx, maxlen);
		if (ret != XST_SUCCESS)
			return -1;

		ret = dma_setup_tx(xdma, &bd_tx);
		if (ret != XST_SUCCESS)
			return -1;
	}

	/* Enable completion interrupts for both channels */
	XAxiDma_IntrEnable(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
	XAxiDma_IntrEnable(xdma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

	return 0;
}