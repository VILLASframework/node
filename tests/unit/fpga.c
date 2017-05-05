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

#include <villas/super_node.h>
#include <villas/utils.h>
#include <villas/nodes/fpga.h>

#include <villas/fpga/ip.h>
#include <villas/fpga/card.h>
#include <villas/fpga/vlnv.h>

#include <villas/fpga/ips/intc.h>
#include <villas/fpga/ips/timer.h>

#define TEST_CONFIG	"/villas/etc/fpga.conf"
#define TEST_LEN	0x1000

#define CPU_HZ		3392389000
#define FPGA_AXI_HZ	125000000

static struct list cards;
static struct fpga_card *card;
static struct super_node sn = { .state = STATE_DESTROYED };
static struct pci pci;
static struct vfio_container vc;

static void init()
{
	int ret;
	config_setting_t *cfg_root;

	ret = super_node_init(&sn);
	cr_assert_eq(ret, 0, "Failed to initialize Supernode");

	ret = super_node_parse_uri(&sn, TEST_CONFIG);
	cr_assert_eq(ret, 0, "Failed to parse configuration");

	ret = super_node_check(&sn);
	cr_assert_eq(ret, 0, "Failed to check configuration");

	cfg_root = config_root_setting(&sn.cfg);
	cr_assert_neq(cfg_root, NULL);

	ret = pci_init(&pci);
	cr_assert_eq(ret, 0, "Failed to initialize PCI sub-system");

	ret = vfio_init(&vc);
	cr_assert_eq(ret, 0, "Failed to initiliaze VFIO sub-system");

	/* Parse FPGA configuration */
	list_init(&cards);
	ret = fpga_card_parse_list(&cards, cfg_root);
	cr_assert_eq(ret, 0, "Failed to parse FPGA config");

	card = list_lookup(&cards, "vc707");
	cr_assert(card, "FPGA card not found");

	if (criterion_options.logging_threshold < CRITERION_IMPORTANT)
		fpga_card_dump(card);
}

static void fini()
{
	int ret;

	ret = fpga_card_destroy(card);
	cr_assert_eq(ret, 0, "Failed to de-initilize FPGA");

	super_node_destroy(&sn);
}

TestSuite(fpga,
	.init = init,
	.fini = fini,
	.description = "VILLASfpga",
	.disabled = true);

Test(fpga, intc, .description = "Interrupt Controller")
{
	int ret;
	uint32_t isr;

	cr_assert(card->intc);

	ret = intc_enable(card->intc, 0xFF00, 0);
	cr_assert_eq(ret, 0, "Failed to enable interrupt");

	/* Fake IRQs in software by writing to ISR */
	XIntc_Out32((uintptr_t) card->map + card->intc->baseaddr + XIN_ISR_OFFSET, 0xFF00);

	/* Wait for 8 SW triggered IRQs */
	for (int i = 0; i < 8; i++)
		intc_wait(card->intc, i+8);

	/* Check ISR if all SW IRQs have been deliverd */
	isr = XIntc_In32((uintptr_t) card->map + card->intc->baseaddr + XIN_ISR_OFFSET);

	ret = intc_disable(card->intc, 0xFF00);
	cr_assert_eq(ret, 0, "Failed to disable interrupt");

	cr_assert_eq(isr & 0xFF00, 0); /* ISR should get cleared by MSI_Grant_signal */
}

Test(fpga, xsg, .description = "XSG: multiply_add")
{
	int ret;
	double factor, err = 0;

	struct fpga_ip *ip, *dma;
	struct model_param *p;
	struct dma_mem mem;

	ip = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { NULL, "sysgen", "xsg_multiply", NULL });
	cr_assert(ip);

	dma = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_dma", NULL });
	cr_assert(dma);

	struct model *model = ip->_vd;

	p = list_lookup(&model->parameters, "factor");
	if (!p)
		error("Missing parameter 'factor' for model '%s'", ip->name);

	ret = model_param_read(p, &factor);
	cr_assert_eq(ret, 0, "Failed to read parameter 'factor' from model '%s'", ip->name);

	info("Model param: factor = %f", factor);

	ret = switch_connect(card->sw, dma, ip);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_connect(card->sw, ip, dma);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = dma_alloc(dma, &mem, 0x1000, 0);
	cr_assert_eq(ret, 0, "Failed to allocate DMA memory");

	float *src = (float *) mem.base_virt;
	float *dst = (float *) mem.base_virt + 0x800;

	for (int i = 0; i < 6; i++)
		src[i] = 1.1 * (i+1);

	ret = dma_ping_pong(dma, (char *) src, (char *) dst, 6 * sizeof(float));
	cr_assert_eq(ret, 0, "Failed to to ping pong DMA transfer: %d", ret);

	for (int i = 0; i < 6; i++)
		err += abs(factor * src[i] - dst[i]);

	info("Error after FPGA operation: err = %f", err);

	ret = switch_disconnect(card->sw, dma, ip);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_disconnect(card->sw, ip, dma);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = dma_free(dma, &mem);
	cr_assert_eq(ret, 0, "Failed to release DMA memory");

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
	cr_assert_eq(ret, 0, "Failed to enable interrupt");

	ret = switch_connect(card->sw, rtds, hls);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_connect(card->sw, hls, rtds);
	cr_assert_eq(ret, 0, "Failed to configure switch");

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
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_disconnect(card->sw, hls, rtds);
	cr_assert_eq(ret, 0, "Failed to configure switch");
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
	cr_assert_eq(ret, 0, "Failed to enable interrupt");

	ret = switch_connect(card->sw, fifo, fifo);
	cr_assert_eq(ret, 0, "Failed to configure switch");

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
	cr_assert_eq(ret, 0, "Failed to disable interrupt");

	ret = switch_disconnect(card->sw, fifo, fifo);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	/* Compare data */
	cr_assert_eq(memcmp(src, dst, sizeof(src)), 0);
}

Test(fpga, dma, .description = "DMA")
{
	int ret = -1;
	struct dma_mem mem, src, dst;

	for (size_t i = 0; i < list_length(&card->ips); i++) { INDENT
		struct fpga_ip *dm = list_at(&card->ips, i);

		if (fpga_vlnv_cmp(&dm->vlnv, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_dma", NULL }))
			continue; /* skip non DMA IP cores */

		struct dma *dma = dm->_vd;

		/* Simple DMA can only transfer up to 4 kb due to
		 * PCIe page size burst limitation */
		ssize_t len2, len = dma->inst.HasSg ? 64 << 20 : 1 << 2;

		ret = dma_alloc(dm, &mem, 2 * len, 0);
		cr_assert_eq(ret, 0);

		ret = dma_mem_split(&mem, &src, &dst);
		cr_assert_eq(ret, 0);

		/* Get new random data */
		len2 = read_random(src.base_virt, len);
		if (len2 != len)
			serror("Failed to get random data");

		int irq_mm2s = dm->irq;
		int irq_s2mm = dm->irq + 1;

		ret = intc_enable(card->intc, (1 << irq_mm2s) | (1 << irq_s2mm), 0);
		cr_assert_eq(ret, 0, "Failed to enable interrupt");

		ret = switch_connect(card->sw, dm, dm);
		cr_assert_eq(ret, 0, "Failed to configure switch");

		/* Start transfer */
		ret = dma_ping_pong(dm, src.base_phys, dst.base_phys, dst.len);
		cr_assert_eq(ret, 0, "DMA ping pong failed");

		ret = memcmp(src.base_virt, dst.base_virt, src.len);

		info("DMA %s (%s): %s", dm->name, dma->inst.HasSg ? "scatter-gather" : "simple", ret ? RED("failed") : GRN("passed"));

		ret = switch_disconnect(card->sw, dm, dm);
		cr_assert_eq(ret, 0, "Failed to configure switch");

		ret = intc_disable(card->intc, (1 << irq_mm2s) | (1 << irq_s2mm));
		cr_assert_eq(ret, 0, "Failed to disable interrupt");

		ret = dma_free(dm, &mem);
		cr_assert_eq(ret, 0, "Failed to release DMA memory");
	}

	cr_assert_eq(ret, 0);
}

Test(fpga, timer, .description = "Timer Counter")
{
	int ret;
	struct fpga_ip *ip;
	struct timer *tmr;

	ip = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_timer", NULL });
	cr_assert(ip);

	tmr = ip->_vd;

	XTmrCtr *xtmr = &tmr->inst;

	ret = intc_enable(card->intc, (1 << ip->irq), 0);
	cr_assert_eq(ret, 0, "Failed to enable interrupt");

	XTmrCtr_SetOptions(xtmr, 0, XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION);
	XTmrCtr_SetResetValue(xtmr, 0, FPGA_AXI_HZ / 125);
	XTmrCtr_Start(xtmr, 0);

	uint64_t counter = intc_wait(card->intc, ip->irq);
	info("Got IRQ: counter = %ju", counter);

	if (counter == 1)
		return;
	else
		warn("Counter was not 1");

	intc_disable(card->intc, (1 << ip->irq));
	cr_assert_eq(ret, 0, "Failed to disable interrupt");

	return;
}

Test(fpga, rtds_rtt, .description = "RTDS: tight rtt")
{
	int ret;
	struct fpga_ip *ip, *rtds;
	struct dma_mem buf;
	size_t recvlen;

	/* Get IP cores */
	rtds = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "acs.eonerc.rwth-aachen.de", "user", "rtds_axis", NULL });
	cr_assert(rtds);

	ip   = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_dma", NULL });
	cr_assert(ip);

	ret = switch_connect(card->sw, rtds, ip);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_connect(card->sw, ip, rtds);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = dma_alloc(ip, &buf, 0x100, 0);
	cr_assert_eq(ret, 0, "Failed to allocate DMA memory");

	while (1) {

		ret = dma_read(ip, buf.base_phys, buf.len);
		cr_assert_eq(ret, 0, "Failed to start DMA read: %d", ret);

		ret = dma_read_complete(ip, NULL, &recvlen);
		cr_assert_eq(ret, 0, "Failed to complete DMA read: %d", ret);

		ret = dma_write(ip, buf.base_phys, recvlen);
		cr_assert_eq(ret, 0, "Failed to start DMA write: %d", ret);

		ret = dma_write_complete(ip, NULL, NULL);
		cr_assert_eq(ret, 0, "Failed to complete DMA write: %d", ret);
	}

	ret = switch_disconnect(card->sw, rtds, ip);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_disconnect(card->sw, ip, rtds);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = dma_free(ip, &buf);
	cr_assert_eq(ret, 0, "Failed to release DMA memory");
}