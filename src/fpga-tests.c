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

#include <xilinx/xtmrctr.h>
#include <xilinx/xintc.h>
#include <xilinx/xllfifo.h>
#include <xilinx/xaxis_switch.h>
#include <xilinx/xaxidma.h>

#include <villas/utils.h>
#include <villas/nodes/fpga.h>

#include <villas/fpga/ip.h>
#include <villas/fpga/intc.h>

//#include <villas/fpga/cbmodel.h>

#include "config.h"
#include "config-fpga.h"

//#include "cbuilder/cbmodel.h"

#define TEST_LEN 0x1000

#define CPU_HZ 3392389000

/* Forward Declarations */
int fpga_test_intc(struct fpga *f);
int fpga_test_timer(struct fpga *f);
int fpga_test_fifo(struct fpga *f);
int fpga_test_dma(struct fpga *f);
int fpga_test_xsg(struct fpga *f);
int fpga_test_rtds_rtt(struct fpga *f);

int fpga_tests(struct fpga *f)
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
		{ "RTDS: RTT Test", 		fpga_test_rtds_rtt }
	};

	for (int i = 0; i < ARRAY_LEN(tests); i++) {
		ret = tests[i].func(f);

		info("%s: %s", tests[i].name, (ret == 0) ? GRN("passed") : RED("failed"));
	}

	return 0;
}

int fpga_test_intc(struct fpga *f)
{
	uint32_t isr;

	if (!f->intc)
		return -1;

	intc_enable(f->intc, 0xFF00);

	info("ISR %#x", XIntc_In32((uintptr_t) f->map + f->intc->baseaddr + XIN_ISR_OFFSET));
	info("IER %#x", XIntc_In32((uintptr_t) f->map + f->intc->baseaddr + XIN_IER_OFFSET));
	info("IMR %#x", XIntc_In32((uintptr_t) f->map + f->intc->baseaddr + XIN_IMR_OFFSET));
	info("MER %#x", XIntc_In32((uintptr_t) f->map + f->intc->baseaddr + XIN_MER_OFFSET));

	/* Fake IRQs in software by writing to ISR */
	XIntc_Out32((uintptr_t) f->map + f->intc->baseaddr + XIN_ISR_OFFSET, 0xFF00);

	/* Wait for 8 SW triggered IRQs */
	for (int i = 0; i < 8; i++)
		intc_wait(f, i+8);

	/* Check ISR if all SW IRQs have been deliverd */
	isr = XIntc_In32((uintptr_t) f->map + f->intc->baseaddr + XIN_ISR_OFFSET);

	intc_disable(f->intc, 0xFF00);

	return (isr & 0xFF00) ? -1 : 0; /* ISR should get cleared by MSI_Grant_signal */
}

int fpga_test_xsg(struct fpga *f)
{
	struct ip *xsg, *dma;
	int ret;
	double factor, err = 0;

	xsg = ip_vlnv_lookup(&f->ips, NULL, "sysgen", "xsg_multiply", NULL);
	dma = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_dma", NULL);

	/* Check if required IP is available on FPGA */
	if (!dma || !xsg || !dma)
		return -1;

	ip_dump(xsg);

	config_setting_t *cfg_params, *cfg_param;
	struct model_param *p;
	
	cfg_params = config_setting_get_member(xsg->cfg, "parameters");
	if (cfg_params) {
		for (int i = 0; i < config_setting_length(cfg_params); i++) {
			cfg_param = config_setting_get_elem(cfg_params, i);
			
			const char *name = config_setting_name(cfg_param);
			double value = config_setting_get_float(cfg_param);
			
			p = list_lookup(&xsg->model.parameters, name);
			if (!p)
				cerror(cfg_param, "Model %s has no parameter named %s", xsg->name, name);

			model_param_write(p, value);
			info("Param '%s' updated to: %f", p->name, value);
		}
	}

	p = list_lookup(&xsg->model.parameters, "factor");
	if (!p)
		error("Missing parameter 'factor' for model '%s'", xsg->name);
	
	ret = model_param_read(p, &factor);
	if (ret)
		error("Failed to read parameter 'factor' from model '%s'", xsg->name);
	
	info("Model param: factor = %f", factor);

	switch_connect(f->sw, dma, xsg);
	switch_connect(f->sw, xsg, dma);

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

	return err > 1e-3;
}

int fpga_test_hls_dft(struct fpga *f)
{
#if 0
	struct ip *hls, *dma;

	hls = ip_vlnv_lookup(&f->ips, NULL, "hls", NULL, NULL);
	dma = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_dma", NULL);

	/* Check if required IP is available on FPGA */
	if (!dma || !hls || !dma)
		return -1;
#endif

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

	intc_enable(f->intc, (1 << fifo->irq));

	/* Get some random data to compare */
	memset(dst, 0, sizeof(dst));
	ret = read_random((char *) src, sizeof(src));
	if (ret)
		error("Failed to get random data");

	ret = switch_connect(f->sw, fifo, fifo);
	if (ret)
		error("Failed to configure switch");

	len = fifo_write(fifo, (char *) src, sizeof(src));
	if (len != sizeof(src))
		error("Failed to send to FIFO");

	len = fifo_read(fifo, (char *) dst, sizeof(dst));
	if (len != sizeof(dst))
		error("Failed to read from FIFO");

	intc_disable(f->intc, (1 << fifo->irq));

	/* Compare data */
	return memcmp(src, dst, sizeof(src));
}

int fpga_test_dma(struct fpga *f)
{
	struct ip *bram;
	ssize_t len = 0x100;
	int ret;

	/* Get some random data to compare */
	char *src = (char *) f->dma;
	char *dst = (char *) f->dma + len;

	ret = read_random(src, len);
	if (ret)
		error("Failed to get random data");

	list_foreach(struct ip *dma, &f->ips) { INDENT
		if (!ip_vlnv_match(dma, "xilinx.com", "ip", "axi_dma", NULL))
			continue; /* skip non DMA IP cores */

		int irq_mm2s = dma->irq;
		int irq_s2mm = dma->irq + 1;
		
		intc_enable(f->intc, (1 << irq_mm2s) | (1 << irq_s2mm));

		ret = switch_connect(f->sw, dma, dma);
		if (ret)
			error("Failed to configure switch");
		
		if (dma->dma.HasSg) {
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
		
		ret = dma_read_complete(dma, NULL, NULL);
		if (ret)
			error("Failed to complete DMA read");

		ret = dma_write_complete(dma, NULL, NULL);
		if (ret)
			error("Failed to complete DMA write");

		ret = memcmp(src, dst, len);

		info("DMA %s (%s): %s", dma->name, dma->dma.HasSg ? "sg" : "simple", ret ? RED("failed") : GRN("passed"));

		intc_disable(f->intc, (1 << irq_mm2s) | (1 << irq_s2mm));
	}

	return ret;
}

int fpga_test_timer(struct fpga *f)
{
	int ret;
	struct ip *timer;

	timer = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_timer", NULL);
	if (!timer)
		return -1;

	intc_enable(f->intc, (1 << timer->irq));

	XTmrCtr_SetOptions(&timer->timer, 0, XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION);
	XTmrCtr_SetResetValue(&timer->timer, 0, AXI_HZ / 125);
	XTmrCtr_Start(&timer->timer, 0);

	struct pollfd pfd = {
		.fd = f->vd.msi_efds[timer->irq],
		.events = POLLIN
	};

	ret = poll(&pfd, 1, 1000);
	if (ret == 1) {
		uint64_t counter = intc_wait(f, timer->irq);

		info("Got IRQ: counter = %ju", counter);

		if (counter == 1)
			return 0;
		else
			warn("Counter was not 1");
	}

	intc_disable(f->intc, (1 << timer->irq));

	return -1;
}

int fpga_test_rtds_rtt(struct fpga *f)
{
#if 0
	uint32_t tsc = 0;
	ssize_t len;

	intc_enable(f->intc, (1 << MSI_DMA_MM2S) | (1 << MSI_DMA_S2MM));

	/* Dump RTDS AXI Stream state */
	rtds_axis_dump(f->map + BASEADDR_RTDS);

	/* Get some random data to compare */
	uint32_t *data_rx = (uint32_t *) (f->dma);
	uint32_t *data_tx = (uint32_t *) (f->dma + 0x1000);

	/* Setup crossbar switch */
	switch_connect(sw, AXIS_SWITCH_RTDS, switch_port);
	switch_connect(sw, switch_port, AXIS_SWITCH_RTDS);

	/* Disable blocking Overflow status */
	int flags = fcntl(f->vd.msi_efds[MSI_RTDS_OVF], F_GETFL, 0);
	fcntl(f->vd.msi_efds[MSI_RTDS_OVF], F_SETFL, flags | O_NONBLOCK);

	int irq_fifo = f->vd.msi_efds[MSI_FIFO_MM];
	int rtt2_prev, rtt2;

	while (1) {
#if 0		/* Wait for TS */
		tsc += intc_wait(f->intc, MSI_RTDS_TS);
#else
		tsc++;
#endif

#if 0		/* Check for overflow */
		ovfl += intc_wait(f->intc, MSI_RTDS_OVF);
		if (tsc % 20000 == 0)
			warn("Missed %u timesteps", ovfl);
#endif

#if DATAMOVER == RTDS_DM_FIFO
retry:		len = fifo_read(&f->fifo, (char *) data_rx, 0x1000, irq_fifo);
		if (XLlFifo_RxOccupancy(&f->fifo) > 0 )
			goto retry;
#elif DATAMOVER == RTDS_DM_DMA_SIMPLE
		len = dma_read(&f->dma_simple, virt_to_dma(data_rx, f->dma), 0x1000, -1);//f->vd.msi_efds[MSI_DMA_S2MM]);
		uint64_t start = rdtscp();
		XAxiDma_IntrAckIrq(&f->dma_simple, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);
#elif DATAMOVER == RTDS_DM_DMA_SG
		len = dma_read(&f->dma_sg, virt_to_dma(data_rx, f->dma), 0x1000, f->vd.msi_efds[MSI_DMA_S2MM]);
#endif
		values_rx = len / sizeof(uint32_t);

		for (int i = 0; i < values_tx; i++)
			data_tx[i] = data_rx[i];
		
		data_tx[3] = tsc;

		if (data_rx[3] == 0)
			tsc = 0;
		
		int rtt = tsc - data_rx[3];
		
		rtt2_prev = rtt2;
		rtt2 = tsc - data_rx[0];

		rdtsc_sleep(30000, start);

#if DATAMOVER == RTDS_DM_FIFO
		len = fifo_write(&f->fifo, (char *) data_tx, sizeof(uint32_t) * values_tx, irq_fifo);
		if (len != sizeof(uint32_t) * values_tx)
			error("Failed to send to FIFO");
#elif DATAMOVER == RTDS_DM_DMA_SIMPLE
		len = dma_write(&f->dma_simple, virt_to_dma(data_tx, f->dma), sizeof(uint32_t) * values_tx, -1);//f->vd.msi_efds[MSI_DMA_MM2S]);
		XAxiDma_IntrAckIrq(&f->dma_simple, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
#elif DATAMOVER == RTDS_DM_DMA_SG
		len = dma_write(&f->dma_sg, virt_to_dma(data_tx, f->dma), sizeof(uint32_t) * values_tx, f->vd.msi_efds[MSI_DMA_MM2S]);
#endif
	}

	intc_disable(f->intc, (1 << MSI_DMA_MM2S) | (1 << MSI_DMA_S2MM));
#endif
	return 0;
}
