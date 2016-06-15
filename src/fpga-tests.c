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
#include <villas/nodes/vfpga.h>

#include <villas/fpga/ip.h>

//#include <villas/fpga/cbmodel.h>

#include "config.h"
#include "config-fpga.h"

//#include "cbuilder/cbmodel.h"

#define TEST_LEN 0x1000

#define CPU_HZ 3392389000

/* Declarations */
int fpga_test_intc(struct vfpga *f);
int fpga_test_timer(struct vfpga *f);
int fpga_test_fifo(struct vfpga *f);
int fpga_test_dma(struct vfpga *f);
int fpga_test_xsg(struct vfpga *f);
int fpga_test_rtds_rtt(struct vfpga *f);
int fpga_test_rtds_cbuilder(struct vfpga *f);

int fpga_tests(struct vfpga *f)
{
	int ret;
	
	struct {
		const char *name;
		int (*func)(struct vfpga *f);
	} tests[] = {
		{ "Interrupt Controller",	fpga_test_intc },
		{ "Timer Counter",		fpga_test_timer },
		{ "FIFO",			fpga_test_fifo },
		{ "DMA",			fpga_test_dma },
		{ "XSG: multiply_add",		fpga_test_xsg },
		{ "RTDS: RTT Test", 		fpga_test_rtds_rtt },
		{ "RTDS: CBuilder",		fpga_test_rtds_cbuilder }
	};

	for (int i = 0; i < ARRAY_LEN(tests); i++) {
		ret = tests[i].func(f);

		info("%s: %s", tests[i].name, (ret == 0) ? GRN("passed") : RED("failed"));
	}

	return 0;
}

int fpga_test_intc(struct vfpga *f)
{
	uint32_t ier, isr;

	/* Save old IER */
	ier = XIntc_In32(f->baseaddr.intc + XIN_IER_OFFSET);
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier | 0xFF00);

	XIntc_Out32(f->baseaddr.intc + XIN_ISR_OFFSET, 0xFF00);

	debug(3, "Wait for 8 SW triggerd interrupts");
	for (int i = 0; i < 8; i++)
		wait_irq(f->vd.msi_efds[i+8]);

	isr = XIntc_In32(f->baseaddr.intc + XIN_ISR_OFFSET);
	
	/* Restore IER */
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier);

	return (isr & 0xFF00) ? -1 : 0; /* ISR should get cleared by MSI_Grant_signal */
}

int fpga_test_xsg(struct vfpga *f)
{
	struct ip *xsg, *dma, *sw;
	
	xsg = ip_vlnv_lookup(&f->ips, NULL, "xsg", "xsg_multiply_add", NULL);
	sw = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axis_interconnect", NULL);
	dma = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_dma", NULL);

	/* Check if required IP is available on FPGA */
	if (!dma || !xsg || !dma)
		return -1;

	ip_dump(xsg);

	list_foreach(struct model_param *p, &xsg->model.parameters) {
		if (p->direction == MODEL_PARAM_IN) {
			model_param_write(p, p->default_value.flt);
			info("Param '%s' updated to: %f", p->name, p->default_value.flt);
		}
	}

	switch_connect(&sw->sw, dma->port, xsg->port);
	switch_connect(&sw->sw, xsg->port, dma->port);

	float *src = (float *) f->dma;
	float *dst = (float *) f->dma + TEST_LEN;
			
	for (int i = 0; i < 6; i++)
		src[i] = 1.1 * (i+1);

	ssize_t len = dma_ping_pong(&dma->dma, virt_to_dma(src, f->dma), virt_to_dma(dst, f->dma), 6 * sizeof(float), -1);
	if (len != 6 * sizeof(float))
		error("Failed to to ping pong DMA transfer: %zd", len);

	for (int i = 0; i < 6; i++)
		info("src[%d] = %f", i, src[i]);
	for (int i = 0; i < 6; i++)
		info("dst[%d] = %f", i, dst[i]);

	return 0;
}

int fpga_test_hls_dft(struct vfpga *f)
{
	struct ip *hls, *sw, *dma;

	hls = ip_vlnv_lookup(&f->ips, NULL, "hls", NULL, NULL);
	sw = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axis_interconnect", NULL);
	dma = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_dma", NULL);

	/* Check if required IP is available on FPGA */
	if (!dma || !hls || !dma)
		return -1;


	return 0;
}

int fpga_test_fifo(struct vfpga *f)
{
	int ret;
	ssize_t len;
	char src[255], dst[255];
	struct ip *fifo, *sw;
	uint32_t ier;

	fifo = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_fifo_mm", NULL);
	if (!fifo)
		return -1;

	sw = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axis_interconnect", NULL);
	if (!sw)
		return -1;

	/* Save old IER */
	ier = XIntc_In32(f->baseaddr.intc + XIN_IER_OFFSET);
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier | (1 << fifo->irq));

	/* Get some random data to compare */
	memset(dst, 0, sizeof(dst));
	ret = read_random((char *) src, sizeof(src));
	if (ret)
		error("Failed to get random data");

	ret = switch_connect(&sw->sw, fifo->port, fifo->port);
	if (ret)
		error("Failed to configure switch");

	len = fifo_write(&fifo->fifo_mm, (char *) src, sizeof(src), f->vd.msi_efds[fifo->irq]);
	if (len != sizeof(src))
		error("Failed to send to FIFO");

	len = fifo_read(&fifo->fifo_mm, (char *) dst, sizeof(dst), f->vd.msi_efds[fifo->irq]);
	if (len != sizeof(dst))
		error("Failed to read from FIFO");

	/* Restore IER */
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier);

	/* Compare data */
	return memcmp(src, dst, sizeof(src));
}

int fpga_test_dma(struct vfpga *f)
{
	struct ip *dma, *sw;
	ssize_t len;
	int ret;
	uint32_t ier;

	dma = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_dma", NULL);
	if (!dma)
		return -1;

	sw = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axis_interconnect", NULL);
	if (!sw)
		return -1;

	int irq_mm2s = dma->irq;
	int irq_s2mm = dma->irq + 1;

	/* Save old IER */
	ier = XIntc_In32(f->baseaddr.intc + XIN_IER_OFFSET);
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier | (1 << irq_mm2s) | (1 << irq_s2mm));

	ret = switch_connect(&sw->sw, dma->port, dma->port);
	if (ret)
		error("Failed to configure switch");

#ifdef HAS_DMA_SG
	/* Memory for buffer descriptors and RX */
	struct dma_mem bd = {
		.base_virt = (uintptr_t) f->map + BASEADDR_BRAM,
		.base_phys = BASEADDR_BRAM,
		.len = 8 * 1024
	};
	
	struct dma_mem rx = {
		.base_virt = (uintptr_t) f->dma + TEST_LEN,
		.base_phys = BASEADDR_HOST + TEST_LEN,
		.len = TEST_LEN
	};
#endif

	/* Get some random data to compare */
	char *src = (char *) f->dma;
	char *dst = (char *) f->dma + TEST_LEN;

	ret = read_random(src, TEST_LEN);
	if (ret)
		error("Failed to get random data");

#if 0
	len = dma_write(&f->dma_simple, virt_to_dma(src, f->dma), TEST_LEN, -1);//f->vd.msi_efds[irq_mm2s]);
	if (len != TEST_LEN)
		error("Failed to send to DMAC: %zd", len);

	len = dma_read(&f->dma_simple, virt_to_dma(dst, f->dma), TEST_LEN, -1);//f->vd.msi_efds[irq_s2mm]);
	if (len != TEST_LEN)
		error("Failed to send to DMAC: %zd", len);
#else
	len = dma_ping_pong(&dma->dma, virt_to_dma(src, f->dma), virt_to_dma(dst, f->dma), TEST_LEN, f->vd.msi_efds[irq_s2mm]);
	if (len != TEST_LEN)
		error("Failed to to ping pong DMA transfer: %zd", len);
#endif

	/* Restore IER */
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier);

	/* Check received data */
	return memcmp(src, dst, TEST_LEN);
}

int fpga_test_timer(struct vfpga *f)
{
	int ret;
	struct ip *timer;
	uint32_t ier;

	timer = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_timer", NULL);
	if (!timer)
		return -1;

	/* Save old IER */
	ier = XIntc_In32(f->baseaddr.intc + XIN_IER_OFFSET);
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier | (1 << timer->irq));

	XTmrCtr_SetOptions(&timer->timer, 0, XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION);
	XTmrCtr_SetResetValue(&timer->timer, 0, AXI_HZ / 125);
	XTmrCtr_Start(&timer->timer, 0);

	struct pollfd pfd = {
		.fd = f->vd.msi_efds[timer->irq],
		.events = POLLIN
	};

	ret = poll(&pfd, 1, 1000);
	if (ret == 1) {
		uint64_t counter = wait_irq(f->vd.msi_efds[timer->irq]);

		info("Got IRQ: counter = %ju", counter);

		if (counter == 1)
			return 0;
		else
			warn("Counter was not 1");
	}

	/* Restore IER */
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier);

	return -1;
}

int fpga_test_rtds_cbuilder(struct vfpga *f)
{
#if 0
	int ret;
	int values_tx = 1;
	int values_rx;
	uint32_t tsc = 0, ovfl = 0;
	ssize_t len;
	uint32_t ier;
	struct ip *rtds, *dma;
	
	int irq_rtds_ovf = rtds->irq + 1;

	rtds = ip_vlnv_lookup(&f->ips, "acs.eonerc.rwth-aachen.de", "user", "rtds_axis", NULL);
	if (!rtds)
		return -1;

	dma = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_dma", NULL);
	if (!dma)
		return -1;

	/* Save old IER */
	ier = XIntc_In32(f->baseaddr.intc + XIN_IER_OFFSET);
//	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier | (1 << MSI_DMA_MM2S) | (1 << MSI_DMA_S2MM));
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier | (1 << irq_rtds_ovf);

	/* Dump RTDS AXI Stream state */
	rtds_axis_dump(f->map + rtds->baseaddr);

	/* Get some random data to compare */
	float *data_rx = (uint32_t *) (f->dma);
	float *data_tx = (uint32_t *) (f->dma + 0x1000);

	/* Setup crossbar switch */
	switch_connect(&sw->sw, rtds->port, dma->port);
	switch_connect(&sw->sw, dma->port, rtds->port);

	/* Disable blocking Overflow status */
	int flags = fcntl(f->vd.msi_efds[irq_rtds_ovf], F_GETFL, 0);
	fcntl(f->vd.msi_efds[irq_rtds_ovf], F_SETFL, flags | O_NONBLOCK);
	
	/* Initialize CBuilder model */
	double mdl_dt = rtds_axis_dt(f->map + BASEADDR_RTDS);
	double mdl_params[] = {
		1,	/**< R2 = 1 Ohm */
		0.001	/**< C2 = 1000 uF */
	};
	double mdl_inputs[1];
	double mdl_outputs[1];

	cbmodel_init(&mdl_params, mdl_dt);

	while (1) {
		/* Check for overflow */
		ovfl += wait_irq(f->vd.msi_efds[MSI_RTDS_OVF]);
		if (tsc % (int) (1.0 / mdl_dt) == 0)
			warn("Missed %u timesteps", ovfl);
		
		break;

		/* Read data from RTDS */
		len = dma_read(&dma->dma, virt_to_dma(data_rx, f->dma), 0x1000, -1);
		XAxiDma_IntrAckIrq(&dma->dma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);

		values_rx = len / sizeof(uint32_t);

		mdl_inputs[0] = data_rx[0]; /* cast to double */
		
		/* Run CBuilder model */
		cbmodel_step(mdl_inputs, mdl_outputs);

		data_tx[0] = mdl_outputs[0]; /* cast to float */

		/* Send data to RTDS */
		len = dma_write(&dma->dma, virt_to_dma(data_tx, f->dma), sizeof(uint32_t) * values_tx, -1);
		XAxiDma_IntrAckIrq(&dma->dma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DMA_TO_DEVICE);
	}

	/* Restore IER */
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier);
#endif
	return 0;
}

int fpga_test_rtds_rtt(struct vfpga *f)
{
#if 0
	uint32_t tsc = 0;
	ssize_t len;
	uint32_t ier;

	/* Save old IER */
	ier = XIntc_In32(f->baseaddr.intc + XIN_IER_OFFSET);
//	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier | (1 << MSI_DMA_MM2S) | (1 << MSI_DMA_S2MM));

	/* Dump RTDS AXI Stream state */
	rtds_axis_dump(f->map + BASEADDR_RTDS);

	/* Get some random data to compare */
	uint32_t *data_rx = (uint32_t *) (f->dma);
	uint32_t *data_tx = (uint32_t *) (f->dma + 0x1000);

	int switch_port;
#if DATAMOVER == RTDS_DM_FIFO
	switch_port = AXIS_SWITCH_FIFO_MM;
#elif DATAMOVER == RTDS_DM_DMA_SIMPLE
	switch_port = AXIS_SWITCH_DMA_SIMPLE;
#elif DATAMOVER == RTDS_DM_DMA_SG
	switch_port = AXIS_SWITCH_DMA_SG;
#endif

	/* Setup crossbar switch */
	switch_connect(&f->sw, AXIS_SWITCH_RTDS, switch_port);
	switch_connect(&f->sw, switch_port, AXIS_SWITCH_RTDS);

	/* Disable blocking Overflow status */
	int flags = fcntl(f->vd.msi_efds[MSI_RTDS_OVF], F_GETFL, 0);
	fcntl(f->vd.msi_efds[MSI_RTDS_OVF], F_SETFL, flags | O_NONBLOCK);

	int irq_fifo = f->vd.msi_efds[MSI_FIFO_MM];
	int rtt2_prev, rtt2;

	while (1) {
#if 0		/* Wait for TS */
		tsc += wait_irq(f->vd.msi_efds[MSI_RTDS_TS]);
#else
		tsc++;
#endif

#if 0		/* Check for overflow */
		ovfl += wait_irq(f->vd.msi_efds[MSI_RTDS_OVF]);
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

	/* Restore IER */
	XIntc_Out32(f->baseaddr.intc + XIN_IER_OFFSET, ier);
#endif
	return 0;
}
