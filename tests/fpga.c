/** Test procedures for VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 *********************************************************************************/

#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>

#include <criterion/criterion.h>
#include <criterion/options.h>

#include <xilinx/xtmrctr.h>

#include <villas/cfg.h>
#include <villas/utils.h>
#include <villas/nodes/fpga.h>

#include <villas/fpga/ip.h>
#include <villas/fpga/card.h>
#include <villas/fpga/vlnv.h>

#include <villas/fpga/ips/intc.h>
#include <villas/fpga/ips/timer.h>

#include "config.h"

#define TEST_CONFIG "/villas/etc/fpga.conf"
#define TEST_LEN 0x1000

#define CPU_HZ 3392389000

static struct fpga_card *card;

static struct cfg cfg;

static void init()
{
	int ret;
	int argc = 1;
	char *argv[] =  { "tests" };
	config_setting_t *cfg_root;
	
	cfg_parse(&cfg, TEST_CONFIG);

	cfg_root = config_root_setting(&cfg.cfg);

	ret = fpga_init(argc, argv, cfg_root);
	cr_assert_eq(ret, 0, "Failed to initilize FPGA");

	card = fpga_lookup_card("vc707");

	if (criterion_options.logging_threshold < CRITERION_IMPORTANT)
		fpga_card_dump(card);
}

static void fini()
{
	int ret;

	ret = fpga_card_destroy(card);
	cr_assert_eq(ret, 0, "Failed to de-initilize FPGA");
	
	cfg_destroy(&cfg);
}


TestSuite(fpga,
	.init = init,
	.fini = fini,
	.description = "VILLASfpga");

Test(fpga, intc, .description = "Interrupt Controller")
{
	int ret;
	uint32_t isr;

	cr_assert(card->intc);

	ret = intc_enable(card->intc, 0xFF00, 0);
	if (ret)
		error("Failed to enable interrupt");

	/* Fake IRQs in software by writing to ISR */
	XIntc_Out32((uintptr_t) card->map + card->intc->baseaddr + XIN_ISR_OFFSET, 0xFF00);

	/* Wait for 8 SW triggered IRQs */
	for (int i = 0; i < 8; i++)
		intc_wait(card->intc, i+8);

	/* Check ISR if all SW IRQs have been deliverd */
	isr = XIntc_In32((uintptr_t) card->map + card->intc->baseaddr + XIN_ISR_OFFSET);

	ret = intc_disable(card->intc, 0xFF00);
	if (ret)
		error("Failed to disable interrupt");

	cr_assert_eq(isr & 0xFF00, 0); /* ISR should get cleared by MSI_Grant_signal */
}

Test(fpga, xsg, .description = "XSG: multiply_add")
{
	int ret;
	double factor, err = 0;

	struct fpga_ip *xsg, *dma;
	struct model_param *p;
	struct dma_mem mem;

	xsg = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { NULL, "sysgen", "xsg_multiply", NULL });
	dma = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_dma", NULL });

	/* Check if required IP is available on FPGA */
	cr_assert_neq(!dma || !xsg || !dma, 0);

	p = list_lookup(&xsg->model.parameters, "factor");
	if (!p)
		error("Missing parameter 'factor' for model '%s'", xsg->name);
	
	ret = model_param_read(p, &factor);
	if (ret)
		error("Failed to read parameter 'factor' from model '%s'", xsg->name);
	
	info("Model param: factor = %f", factor);

	ret = switch_connect(card->sw, dma, xsg);
	if (ret)
		error("Failed to configure switch");
	ret = switch_connect(card->sw, xsg, dma);
	if (ret)
		error("Failed to configure switch");

	ret = dma_alloc(dma, &mem, 0x1000, 0);
	if (ret)
		error("Failed to allocate DMA memory");
	
	float *src = (float *) mem.base_virt;
	float *dst = (float *) mem.base_virt + 0x800;

	for (int i = 0; i < 6; i++)
		src[i] = 1.1 * (i+1);

	ret = dma_ping_pong(dma, (char *) src, (char *) dst, 6 * sizeof(float));
	if (ret)
		error("Failed to to ping pong DMA transfer: %d", ret);

	for (int i = 0; i < 6; i++)
		err += abs(factor * src[i] - dst[i]);

	info("Error after FPGA operation: err = %f", err);

	ret = switch_disconnect(card->sw, dma, xsg);
	if (ret)
		error("Failed to configure switch");
	ret = switch_disconnect(card->sw, xsg, dma);
	if (ret)
		error("Failed to configure switch");

	ret = dma_free(dma, &mem);
	if (ret)
		error("Failed to release DMA memory");

	cr_assert(err < 1e-3);
}

Test(fpga, hls_dft, .description = "HLS: hls_dft")
{
	int ret;
	struct fpga_ip *hls, *rtds;
	
	rtds = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "acs.eonerc.rwth-aachen.de", "user", "rtds_axis", NULL });
	hls = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { NULL, "hls", "hls_dft", NULL });

	/* Check if required IP is available on FPGA */
	cr_assert(hls && rtds);

	ret = intc_enable(card->intc, (1 << rtds->irq), 0);
	if (ret)
		error("Failed to enable interrupt");
	
	ret = switch_connect(card->sw, rtds, hls);
	if (ret)
		error("Failed to configure switch");
	ret = switch_connect(card->sw, hls, rtds);
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

	ret = switch_disconnect(card->sw, rtds, hls);
	if (ret)
		error("Failed to configure switch");
	ret = switch_disconnect(card->sw, hls, rtds);
	if (ret)
		error("Failed to configure switch");
}

Test(fpga, fifo, .description = "FIFO")
{
	int ret;
	ssize_t len;
	char src[255], dst[255];
	struct fpga_ip *fifo;

	fifo = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_fifo_mm_s", NULL });
	cr_assert(fifo);

	ret = intc_enable(card->intc, (1 << fifo->irq), 0);
	if (ret)
		error("Failed to enable interrupt");

	ret = switch_connect(card->sw, fifo, fifo);
	if (ret)
		error("Failed to configure switch");

	/* Get some random data to compare */
	memset(dst, 0, sizeof(dst));
	len = read_random((char *) src, sizeof(src));
	if (len != sizeof(src))
		error("Failed to get random data");

	len = fifo_write(fifo, (char *) src, sizeof(src));
	if (len != sizeof(src))
		error("Failed to send to FIFO");

	len = fifo_read(fifo, (char *) dst, sizeof(dst));
	if (len != sizeof(dst))
		error("Failed to read from FIFO");

	ret = intc_disable(card->intc, (1 << fifo->irq));
	if (ret)
		error("Failed to disable interrupt");

	ret = switch_disconnect(card->sw, fifo, fifo);
	if (ret)
		error("Failed to configure switch");

	/* Compare data */
	cr_assert_eq(memcmp(src, dst, sizeof(src)), 0);
}

Test(fpga, dma, .description = "DMA")
{
	int ret = -1;
	struct dma_mem mem, src, dst;

	list_foreach(struct fpga_ip *dma, &card->ips) { INDENT
		if (fpga_vlnv_cmp(&dma->vlnv, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_dma", NULL }))
			continue; /* skip non DMA IP cores */
		
		/* Simple DMA can only transfer up to 4 kb due to
		 * PCIe page size burst limitation */
		ssize_t len2, len = dma->dma.inst.HasSg ? 64 << 20 : 1 << 2;

		ret = dma_alloc(dma, &mem, 2 * len, 0);
		cr_assert_eq(ret, 0);

		ret = dma_mem_split(&mem, &src, &dst);
		cr_assert_eq(ret, 0);

		/* Get new random data */
		len2 = read_random(src.base_virt, len);
		if (len2 != len)
			serror("Failed to get random data");

		int irq_mm2s = dma->irq;
		int irq_s2mm = dma->irq + 1;

		ret = intc_enable(card->intc, (1 << irq_mm2s) | (1 << irq_s2mm), 0);
		if (ret)
			error("Failed to enable interrupt");

		ret = switch_connect(card->sw, dma, dma);
		if (ret)
			error("Failed to configure switch");
		
		/* Start transfer */
		ret = dma_ping_pong(dma, src.base_phys, dst.base_phys, dst.len);
		if (ret)
			error("DMA ping pong failed");

		ret = memcmp(src.base_virt, dst.base_virt, src.len);

		info("DMA %s (%s): %s", dma->name, dma->dma.inst.HasSg ? "scatter-gather" : "simple", ret ? RED("failed") : GRN("passed"));

		ret = switch_disconnect(card->sw, dma, dma);
		if (ret)
			error("Failed to configure switch");

		ret = intc_disable(card->intc, (1 << irq_mm2s) | (1 << irq_s2mm));
		if (ret)
			error("Failed to disable interrupt");
		
		ret = dma_free(dma, &mem);
		if (ret)
			error("Failed to release DMA memory");
	}

	cr_assert_eq(ret, 0);
}

Test(fpga, timer, .description = "Timer Counter")
{
	int ret;
	struct fpga_ip *tmr;

	tmr = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_timer", NULL });
	cr_assert(tmr);
	
	XTmrCtr *xtmr = &tmr->timer.inst;

	ret = intc_enable(card->intc, (1 << tmr->irq), 0);
	if (ret)
		error("Failed to enable interrupt");

	XTmrCtr_SetOptions(xtmr, 0, XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION);
	XTmrCtr_SetResetValue(xtmr, 0, FPGA_AXI_HZ / 125);
	XTmrCtr_Start(xtmr, 0);

	uint64_t counter = intc_wait(card->intc, tmr->irq);
	info("Got IRQ: counter = %ju", counter);

	if (counter == 1)
		return;
	else
		warn("Counter was not 1");

	intc_disable(card->intc, (1 << tmr->irq));
	if (ret)
		error("Failed to disable interrupt");

	return;
}

Test(fpga, rtds_rtt, .description = "RTDS: tight rtt")
{
	int ret;
	struct fpga_ip *dma, *rtds;
	struct dma_mem buf;
	size_t recvlen;

	/* Get IP cores */
	rtds = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "acs.eonerc.rwth-aachen.de", "user", "rtds_axis", NULL });
	dma = list_lookup(&card->ips, "dma_1");

	/* Check if required IP is available on FPGA */
	cr_assert (dma && rtds);

	ret = switch_connect(card->sw, rtds, dma);
	if (ret)
		error("Failed to configure switch");
	ret = switch_connect(card->sw, dma, rtds);
	if (ret)
		error("Failed to configure switch");

	ret = dma_alloc(dma, &buf, 0x100, 0);
	if (ret)
		error("Failed to allocate DMA memory");

	while (1) {
		
		ret = dma_read(dma, buf.base_phys, buf.len);
		if (ret)
			error("Failed to start DMA read: %d", ret);

		ret = dma_read_complete(dma, NULL, &recvlen);
		if (ret)
			error("Failed to complete DMA read: %d", ret);

		ret = dma_write(dma, buf.base_phys, recvlen);
		if (ret)
			error("Failed to start DMA write: %d", ret);
		
		ret = dma_write_complete(dma, NULL, NULL);
		if (ret)
			error("Failed to complete DMA write: %d", ret);
	}

	ret = switch_disconnect(card->sw, rtds, dma);
	if (ret)
		error("Failed to configure switch");

	ret = switch_disconnect(card->sw, dma, rtds);
	if (ret)
		error("Failed to configure switch");
	
	ret = dma_free(dma, &buf);
	if (ret)
		error("Failed to release DMA memory");
}