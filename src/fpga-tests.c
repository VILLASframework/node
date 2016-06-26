/** Test procedures for VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <unistd.h>

#include <xilinx/xtmrctr.h>
#include <xilinx/xintc.h>
#include <xilinx/xllfifo.h>
#include <xilinx/xaxis_switch.h>
#include <xilinx/xaxidma.h>

#include <villas/utils.h>
#include <villas/nodes/fpga.h>

#include <villas/fpga/ip.h>
#include <villas/fpga/intc.h>

#include "config.h"
#include "config-fpga.h"

#define TEST_LEN 0x1000

#define CPU_HZ 3392389000

/* Forward Declarations */
int fpga_test_intc(struct fpga *f);
int fpga_test_timer(struct fpga *f);
int fpga_test_fifo(struct fpga *f);
int fpga_test_dma(struct fpga *f);
int fpga_test_xsg(struct fpga *f);
int fpga_test_hls_dft(struct fpga *f);
int fpga_test_rtds_rtt(struct fpga *f);

int fpga_tests(int argc, char *argv[], struct fpga *f)
{
	int ret;
	
	struct {
		const char *name;
		int (*func)(struct fpga *f);
	} tests[] = {
		{ "Interrupt Controller",	fpga_test_intc },
		{ "Timer Counter",		fpga_test_timer },
		{ "FIFO",			fpga_test_fifo },
		{ "DMA",			fpga_test_dma },
		{ "XSG: multiply_add",		fpga_test_xsg },
		{ "HLS: hls_dft",		fpga_test_hls_dft },
		{ "RTDS: tight rtt",		fpga_test_rtds_rtt }
	};

	/* We need to overwrite the AXI4-Stream switch configuration from the
	 * config file. Therefore we first disable everything here. */
	XAxisScr_RegUpdateDisable(&f->sw->sw.inst);
	XAxisScr_MiPortDisableAll(&f->sw->sw.inst);
	XAxisScr_RegUpdateEnable(&f->sw->sw.inst);

	for (int i = 0; i < ARRAY_LEN(tests); i++) {
		ret = tests[i].func(f);

		info("%s: %s", tests[i].name, (ret == 0) ? GRN("passed") : RED("failed"));
	}

	return 0;
}

int fpga_test_intc(struct fpga *f)
{
	int ret;
	uint32_t isr;

	if (!f->intc)
		return -1;

	ret = intc_enable(f->intc, 0xFF00, 0);
	if (ret)
		error("Failed to enable interrupt");

	/* Fake IRQs in software by writing to ISR */
	XIntc_Out32((uintptr_t) f->map + f->intc->baseaddr + XIN_ISR_OFFSET, 0xFF00);

	/* Wait for 8 SW triggered IRQs */
	for (int i = 0; i < 8; i++)
		intc_wait(f->intc, i+8, 0);

	/* Check ISR if all SW IRQs have been deliverd */
	isr = XIntc_In32((uintptr_t) f->map + f->intc->baseaddr + XIN_ISR_OFFSET);

	ret = intc_disable(f->intc, 0xFF00);
	if (ret)
		error("Failed to disable interrupt");

	return (isr & 0xFF00) ? -1 : 0; /* ISR should get cleared by MSI_Grant_signal */
}

int fpga_test_xsg(struct fpga *f)
{
	struct ip *xsg, *dma;
	struct model_param *p;
	int ret;
	double factor, err = 0;

	xsg = ip_vlnv_lookup(&f->ips, NULL, "sysgen", "xsg_multiply", NULL);
	dma = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_dma", NULL);

	/* Check if required IP is available on FPGA */
	if (!dma || !xsg || !dma)
		return -1;

	p = list_lookup(&xsg->model.parameters, "factor");
	if (!p)
		error("Missing parameter 'factor' for model '%s'", xsg->name);
	
	ret = model_param_read(p, &factor);
	if (ret)
		error("Failed to read parameter 'factor' from model '%s'", xsg->name);
	
	info("Model param: factor = %f", factor);

	ret = switch_connect(f->sw, dma, xsg);
	if (ret)
		error("Failed to configure switch");
	ret = switch_connect(f->sw, xsg, dma);
	if (ret)
		error("Failed to configure switch");

	float *src = (float *) f->dma;
	float *dst = (float *) f->dma + TEST_LEN;

	for (int i = 0; i < 6; i++)
		src[i] = 1.1 * (i+1);

	ret = dma_ping_pong(dma, (char *) src, (char *) dst, 6 * sizeof(float));
	if (ret)
		error("Failed to to ping pong DMA transfer: %d", ret);

	for (int i = 0; i < 6; i++)
		err += abs(factor * src[i] - dst[i]);

	info("Error after FPGA operation: err = %f", err);

	ret = switch_disconnect(f->sw, dma, xsg);
	if (ret)
		error("Failed to configure switch");
	ret = switch_disconnect(f->sw, xsg, dma);
	if (ret)
		error("Failed to configure switch");

	return err > 1e-3;
}

int fpga_test_hls_dft(struct fpga *f)
{
	int ret;
	struct ip *hls, *rtds;
	
	rtds = ip_vlnv_lookup(&f->ips, "acs.eonerc.rwth-aachen.de", "user", "rtds_axis", NULL);
	hls = ip_vlnv_lookup(&f->ips, NULL, "hls", "hls_dft", NULL);

	/* Check if required IP is available on FPGA */
	if (!hls || !rtds)
		return -1;

	ret = intc_enable(f->intc, (1 << rtds->irq), 0);
	if (ret)
		error("Failed to enable interrupt");
	
	ret = switch_connect(f->sw, rtds, hls);
	if (ret)
		error("Failed to configure switch");
	ret = switch_connect(f->sw, hls, rtds);
	if (ret)
		error("Failed to configure switch");

	while(1) {
		/* Dump RTDS AXI Stream state */
		rtds_axis_dump(rtds);
		sleep(1);
	}
#if 0
	int len = 2000;
	int NSAMPLES = 400;
	float src[len], dst[len];
	
	for (int i = 0; i < len; i++) {
		src[i] = 4 + 5.0 * sin(2.0 * M_PI * 1 * i / NSAMPLES) + 
		             2.0 * sin(2.0 * M_PI * 2 * i / NSAMPLES) + 
			     1.0 * sin(2.0 * M_PI * 5 * i / NSAMPLES) + 
			     0.5 * sin(2.0 * M_PI * 9 * i / NSAMPLES) + 
			     0.2 * sin(2.0 * M_PI * 15 * i / NSAMPLES);
	
		fifo_write()
	}
#endif

	ret = switch_disconnect(f->sw, rtds, hls);
	if (ret)
		error("Failed to configure switch");
	ret = switch_disconnect(f->sw, hls, rtds);
	if (ret)
		error("Failed to configure switch");

	return 0;
}

int fpga_test_fifo(struct fpga *f)
{
	int ret;
	ssize_t len;
	char src[255], dst[255];
	struct ip *fifo;

	fifo = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_fifo_mm_s", NULL);
	if (!fifo)
		return -1;

	ret = intc_enable(f->intc, (1 << fifo->irq), 0);
	if (ret)
		error("Failed to enable interrupt");

	ret = switch_connect(f->sw, fifo, fifo);
	if (ret)
		error("Failed to configure switch");

	/* Get some random data to compare */
	memset(dst, 0, sizeof(dst));
	ret = read_random((char *) src, sizeof(src));
	if (ret)
		error("Failed to get random data");

	len = fifo_write(fifo, (char *) src, sizeof(src));
	if (len != sizeof(src))
		error("Failed to send to FIFO");

	len = fifo_read(fifo, (char *) dst, sizeof(dst));
	if (len != sizeof(dst))
		error("Failed to read from FIFO");

	ret = intc_disable(f->intc, (1 << fifo->irq));
	if (ret)
		error("Failed to disable interrupt");

	ret = switch_disconnect(f->sw, fifo, fifo);
	if (ret)
		error("Failed to configure switch");

	/* Compare data */
	return memcmp(src, dst, sizeof(src));
}

int fpga_test_dma(struct fpga *f)
{
	struct ip *bram;
	ssize_t len = 0x100;
	int ret;

	char *src = (char *) f->dma;
	char *dst = (char *) f->dma + len;

	list_foreach(struct ip *dma, &f->ips) { INDENT
		if (!ip_vlnv_match(dma, "xilinx.com", "ip", "axi_dma", NULL))
			continue; /* skip non DMA IP cores */

		/* Get new random data */
		ret = read_random(src, len);
		if (ret)
			error("Failed to get random data");

		int irq_mm2s = dma->irq;
		int irq_s2mm = dma->irq + 1;
		
		ret = intc_enable(f->intc, (1 << irq_mm2s) | (1 << irq_s2mm), 0);
		if (ret)
			error("Failed to enable interrupt");

		ret = switch_connect(f->sw, dma, dma);
		if (ret)
			error("Failed to configure switch");
		
		XAxiDma_Reset(&dma->dma.inst);

		if (dma->dma.inst.HasSg) {
			/* Init BD rings */
			bram = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_bram_ctrl", NULL);
			if (!bram)
				return -3;

			/* Memory for buffer descriptors */
			struct dma_mem bd = {
				.base_virt = (uintptr_t) f->map + bram->baseaddr,
				.base_phys = bram->baseaddr,
				.len = bram->bram.size
			};

			ret = dma_init_rings(dma, &bd);
			if (ret)
				return -4;
		}

		/* Start transfer */
		ret = dma_read(dma, dst, len);
		if (ret)
			error("Failed to start DMA read: %d", ret);

		ret = dma_write(dma, src, len);
		if (ret)
			error("Failed to start DMA write: %d", ret);
	
		size_t recvlen;
		ret = dma_read_complete(dma, NULL, &recvlen);
		if (ret)
			error("Failed to complete DMA read");

		ret = dma_write_complete(dma, NULL, NULL);
		if (ret)
			error("Failed to complete DMA write");

		info("recvlen = %#zx", recvlen);
		
		ret = memcmp(src, dst, len);

		info("DMA %s (%s): %s", dma->name, dma->dma.inst.HasSg ? "scatter-gather" : "simple", ret ? RED("failed") : GRN("passed"));

		ret = switch_disconnect(f->sw, dma, dma);
		if (ret)
			error("Failed to configure switch");

		ret = intc_disable(f->intc, (1 << irq_mm2s) | (1 << irq_s2mm));
		if (ret)
			error("Failed to disable interrupt");
	}

	return ret;
}

int fpga_test_timer(struct fpga *f)
{
	int ret;
	struct ip *tmr;

	tmr = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_timer", NULL);
	if (!tmr)
		return -1;
	
	XTmrCtr *xtmr = &tmr->timer.inst;

	ret = intc_enable(f->intc, (1 << tmr->irq), 0);
	if (ret)
		error("Failed to enable interrupt");

	XTmrCtr_SetOptions(xtmr, 0, XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION);
	XTmrCtr_SetResetValue(xtmr, 0, AXI_HZ / 125);
	XTmrCtr_Start(xtmr, 0);

	struct pollfd pfd = {
		.fd = f->vd.msi_efds[tmr->irq],
		.events = POLLIN
	};

	ret = poll(&pfd, 1, 1000);
	if (ret == 1) {
		uint64_t counter = intc_wait(f->intc, tmr->irq, 0);

		info("Got IRQ: counter = %ju", counter);

		if (counter == 1)
			return 0;
		else
			warn("Counter was not 1");
	}

	intc_disable(f->intc, (1 << tmr->irq));
	if (ret)
		error("Failed to disable interrupt");

	return -1;
}

int fpga_test_rtds_rtt(struct fpga *f)
{
	int ret;
	struct ip *dma, *rtds;

	rtds = ip_vlnv_lookup(&f->ips, "acs.eonerc.rwth-aachen.de", "user", "rtds_axis", NULL);
	dma = list_lookup(&f->ips, "dma_1");

	/* Check if required IP is available on FPGA */
	if (!dma || !rtds)
		return -1;

	ret = switch_connect(f->sw, rtds, dma);
	if (ret)
		error("Failed to configure switch");
	ret = switch_connect(f->sw, dma, rtds);
	if (ret)
		error("Failed to configure switch");

	size_t len = 0x100;
	size_t recvlen;
	char *buf = f->dma;

	while (1) {
		
		ret = dma_read(dma, buf, len);
		if (ret)
			error("Failed to start DMA read: %d", ret);

		ret = dma_read_complete(dma, NULL, &recvlen);
		if (ret)
			error("Failed to complete DMA read: %d", ret);

		ret = dma_write(dma, buf, recvlen);
		if (ret)
			error("Failed to start DMA write: %d", ret);
		
		ret = dma_write_complete(dma, NULL, NULL);
		if (ret)
			error("Failed to complete DMA write: %d", ret);
	}

	ret = switch_disconnect(f->sw, rtds, dma);
	if (ret)
		error("Failed to configure switch");
	ret = switch_disconnect(f->sw, dma, rtds);
	if (ret)
		error("Failed to configure switch");

	return 0;
}