/** Node type: VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "kernel/kernel.h"
#include "kernel/pci.h"

#include "nodes/fpga.h"

#include "config-fpga.h"
#include "utils.h"
#include "timing.h"

struct fpga fpga;
struct vfio_container vc;

int fpga_reset(struct fpga *f)
{
	int ret;
	char state[4096];

	/* Save current state of PCI configuration space */
	ret = pread(f->vd.fd, state, sizeof(state), (off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40);
	if (ret != sizeof(state))
		return -1;

	uint32_t *rst_reg = (uint32_t *) (f->map + f->reset->baseaddr);

	debug(3, "FPGA: reset");
	rst_reg[0] = 1;

	usleep(100000);

	/* Restore previous state of PCI configuration space */
	ret = pwrite(f->vd.fd, state, sizeof(state), (off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40);
	if (ret != sizeof(state))
		return -1;

	/* After reset the value should be zero again */
	if (rst_reg[0])
		return -2;

	return 0;
}

void fpga_dump(struct fpga *f)
{
	char namebuf[128];
	char *name;

	struct pci_access *pacc;

	pacc = pci_get_handle();
	name = pci_lookup_name(pacc, namebuf, sizeof(namebuf), PCI_LOOKUP_DEVICE, fpga.vd.pdev->vendor_id, fpga.vd.pdev->device_id);
	pci_fill_info(fpga.vd.pdev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);	/* Fill in header info we need */

	info("VILLASfpga card: %s", name);
	{ INDENT
		info("Slot: %04x:%02x:%02x.%d", fpga.vd.pdev->domain, fpga.vd.pdev->bus, fpga.vd.pdev->dev, fpga.vd.pdev->func);
		info("Vendor ID: %04x", fpga.vd.pdev->vendor_id);
		info("Device ID: %04x", fpga.vd.pdev->device_id);
		info("Class  ID: %04x", fpga.vd.pdev->device_class);

		info("BAR0 mapped at %p", fpga.map);

		info("IP blocks:");
		list_foreach(struct ip *i, &f->ips) { INDENT
			ip_dump(i);
		}
	}

	vfio_dump(fpga.vd.group->container);
}

struct fpga * fpga_get()
{
	return &fpga;
}

int fpga_parse_card(struct fpga *f, int argc, char * argv[], config_setting_t *cfg)
{
	int ret;
	const char *slot, *id, *err;
	config_setting_t *cfg_ips, *cfg_slot, *cfg_id, *cfg_fpgas;

	/* Default values */
	f->filter.vendor = PCI_VID_XILINX;
	f->filter.device = PCI_PID_VFPGA;

	cfg_fpgas = config_setting_get_member(cfg, "fpga");
	if (!cfg_fpgas)
		cerror(cfg, "Config file is missing 'fpgas' section");
	
	if (!config_setting_is_group(cfg_fpgas))
		cerror(cfg_fpgas, "FPGA configuration section must be a group");
	
	if (config_setting_length(cfg_fpgas) != 1)
		cerror(cfg_fpgas, "Currently, only a single FPGA is currently supported.");
	
	f->cfg = config_setting_get_elem(cfg_fpgas, 0);

	if (!config_setting_lookup_int(cfg, "affinity", &f->affinity))
		f->affinity = 0;

	config_setting_lookup_bool(f->cfg, "do_reset", &f->do_reset);

	cfg_slot = config_setting_get_member(f->cfg, "slot");
	if (cfg_slot) {
		slot = config_setting_get_string(cfg_slot);
		if (slot) {
			err = pci_filter_parse_slot(&f->filter, (char*) slot);
			if (err)
				cerror(cfg_slot, "%s", err);
		}
		else
			cerror(cfg_slot, "Invalid slot format");
	}

	cfg_id = config_setting_get_member(f->cfg, "id");
	if (cfg_id) {
		id = config_setting_get_string(cfg_id);
		if (id) {
			err = pci_filter_parse_id(&f->filter, (char*) id);
			if (err)
				cerror(cfg_id, "%s", err);
		}
		else
			cerror(cfg_slot, "Invalid id format");
	}
	
	cfg_ips = config_setting_get_member(f->cfg, "ips");
	if (!cfg_ips)
		cerror(f->cfg, "FPGA configuration is missing ips section");

	for (int i = 0; i < config_setting_length(cfg_ips); i++) {
		config_setting_t *cfg_ip = config_setting_get_elem(cfg_ips, i);

		struct ip ip = {
			.card = f
		};
	
		ret = ip_parse(&ip, cfg_ip);
		if (ret)
			cerror(cfg_ip, "Failed to parse VILLASfpga IP core");

		list_push(&f->ips, memdup(&ip, sizeof(ip)));
	}

	return 0;
}

int fpga_init(int argc, char * argv[], config_setting_t *cfg)
{
	int ret;
	struct pci_access *pacc;
	struct pci_dev *pdev;
	struct fpga *f;

	/* For now we only support a single VILALSfpga card */
	f = fpga_get();
	list_init(&f->ips);

	pacc = pci_get_handle();
	pci_filter_init(pacc, &f->filter);

	/* Parse FPGA configuration */
	ret = fpga_parse_card(f, argc, argv, cfg);
	if (ret)
		cerror(cfg, "Failed to parse VILLASfpga config");

	/* Check FPGA configuration */
	f->reset = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_gpio", NULL);
	if (!f->reset)
		error("FPGA is missing a reset controller");

	f->intc = ip_vlnv_lookup(&f->ips, "acs.eonerc.rwth-aachen.de", "user", "axi_pcie_intc", NULL);
	if (!f->intc)
		error("FPGA is missing a interrupt controller");

	f->sw = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axis_interconnect", NULL);
	if (!f->sw)
		warn("FPGA is missing an AXI4-Stream switch");

	/* Search for FPGA card */
	pdev = pci_find_device(pacc, &f->filter);
	if (!pdev)
		error("Failed to find PCI device");

	/* Get VFIO handles and details */
	ret = vfio_init(&vc);
	if (ret)
		serror("Failed to initialize VFIO");

	/* Attach PCIe card to VFIO container */
	ret = vfio_pci_attach(&f->vd, &vc, pdev);
	if (ret)
		error("Failed to attach VFIO device");

	/* Map PCIe BAR */
	f->map = vfio_map_region(&f->vd, VFIO_PCI_BAR0_REGION_INDEX);
	if (f->map == MAP_FAILED)
		serror("Failed to mmap() BAR0");

	/* Enable memory access and PCI bus mastering for DMA */
	ret = vfio_pci_enable(&f->vd);
	if (ret)
		serror("Failed to enable PCI device");
	
	/* Reset system? */
	if (f->do_reset) {
		/* Reset / detect PCI device */
		ret = vfio_pci_reset(&f->vd);
		if (ret)
			serror("Failed to reset PCI device");

		ret = fpga_reset(f);
		if (ret)
			error("Failed to reset FGPA card");
	}

	/* Initialize IP cores */
	list_foreach(struct ip *c, &f->ips) {
		ret = ip_init(c);
		if (ret)
			error("Failed to initalize IP core: %s (%u)", c->name, ret);
	}

	return 0;
}

int fpga_deinit()
{
	int ret;

	list_destroy(&fpga.ips, (dtor_cb_t) ip_destroy, true);

	pci_release_handle();

	ret = vfio_destroy(&vc);
	if (ret)
		error("Failed to deinitialize VFIO module");

	return 0;
}

int fpga_parse(struct node *n, config_setting_t *cfg)
{
	struct fpga_dm *d = n->_vd;

	/* There is currently only support for a single FPGA card */
	d->card = fpga_get();

	if (!config_setting_lookup_string(cfg, "datamover", &d->ip_name))
		cerror(cfg, "Node '%s' is missing the 'datamover' setting", node_name(n));

	if (!config_setting_lookup_bool(cfg, "use_irqs", &d->use_irqs))
		d->use_irqs = false;

	return 0;
}

char * fpga_print(struct node *n)
{
	struct fpga_dm *d = n->_vd;
	struct fpga *f = d->card;
	
	if (d->ip)
		return strf("dm=%s (%s:%s:%s:%s) baseaddr=%#jx port=%u slot=%02"PRIx8":%02"PRIx8".%"PRIx8" id=%04"PRIx16":%04"PRIx16,
			d->ip->name, d->ip->vlnv.vendor, d->ip->vlnv.library, d->ip->vlnv.name, d->ip->vlnv.version, d->ip->baseaddr, d->ip->port,
			f->filter.bus, f->filter.device, f->filter.func,
			f->filter.vendor, f->filter.device);
	else
		return strf("dm=%s", d->ip_name);
}

int fpga_get_type(struct ip *c)
{
	if      (ip_vlnv_match(c, "xilinx.com", "ip", "axi_dma", NULL))
		return FPGA_DM_DMA;
	else if (ip_vlnv_match(c, "xilinx.com", "ip", "axi_fifo_mm_s", NULL))
		return FPGA_DM_FIFO;
	else
		return -1;
}

int fpga_open(struct node *n)
{
	int ret;

	struct fpga_dm *d = n->_vd;
	struct fpga *f = d->card;

	d->ip = list_lookup(&d->card->ips, d->ip_name);
	if (!d->ip)
		cerror(n->cfg, "Datamover '%s' is unknown. Please specify it in the fpga.ips section", d->ip_name);

	d->type = fpga_get_type(d->ip);
	if (d->type < 0)
		cerror(n->cfg, "IP '%s' is not a supported datamover", d->ip->name);

	switch_init_paths(f->sw);

	int flags = 0;
	if (!d->use_irqs)
		flags |= INTC_POLLING;

	switch (d->type) {
		case FPGA_DM_DMA:
			/* Map DMA accessible memory */
			ret = dma_alloc(d->ip, &d->dma, 0x1000, 0);
			if (ret)
				return ret;

			intc_enable(f->intc, (1 << (d->ip->irq    )), flags); /* MM2S */
			intc_enable(f->intc, (1 << (d->ip->irq + 1)), flags); /* S2MM */
			break;

		case FPGA_DM_FIFO:
			intc_enable(f->intc, (1 << d->ip->irq),      flags);	/* MM2S & S2MM */
			break;
	}
	

	return 0;
}

int fpga_close(struct node *n)
{
	int ret;

	struct fpga_dm *d = n->_vd;
	struct fpga *f = d->card;

	switch (d->type) {
		case FPGA_DM_DMA:
			intc_disable(f->intc, d->ip->irq);	/* MM2S */
			intc_disable(f->intc, d->ip->irq + 1);	/* S2MM */

			ret = dma_free(d->ip, &d->dma);
			if (ret)
				return ret;

		case FPGA_DM_FIFO:
			if (d->use_irqs)
				intc_disable(f->intc, d->ip->irq);	/* MM2S & S2MM */
	}

	return 0;
}

int fpga_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;

	struct fpga_dm *d = n->_vd;
	struct sample *smp = smps[0];

	size_t recvlen;

	size_t len = SAMPLE_DATA_LEN(64);
	
	/* We dont get a sequence no from the FPGA. Lets fake it */
	smp->sequence = n->received;
	smp->ts.origin = time_now();

	/* Read data from RTDS */
	switch (d->type) {
		case FPGA_DM_DMA:
			ret = dma_read(d->ip, d->dma.base_phys + 0x800, len);
			if (ret)
				return ret;
			
			ret = dma_read_complete(d->ip, NULL, &recvlen);
			if (ret)
				return ret;

			memcpy(smp->data, d->dma.base_virt + 0x800, recvlen);

			smp->length = recvlen / 4;
			return 1;
		case FPGA_DM_FIFO:
			recvlen = fifo_read(d->ip, (char *) smp->data, len);
			
			smp->length = recvlen / 4;
			return 1;
	}

	return -1;
}

int fpga_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	int ret;
	struct fpga_dm *d = n->_vd;
	struct sample *smp = smps[0];

	size_t sentlen;
	size_t len = smp->length * sizeof(smp->data[0]);

	//intc_wait(f->intc, 5, 1);
	
	//if (n->received % 40000 == 0) {
	//	struct timespec now = time_now();
	//	info("proc time = %f", time_delta(&smp->ts.origin, &now));
	//}

	/* Send data to RTDS */
	switch (d->type) {
		case FPGA_DM_DMA:
			memcpy(d->dma.base_virt, smp->data, len);

			ret = dma_write(d->ip, d->dma.base_phys, len);
			if (ret)
				return ret;
			
			ret = dma_write_complete(d->ip, NULL, &sentlen);
			if (ret)
				return ret;

			//info("Sent %u bytes to FPGA", sentlen);

			return 1;
		case FPGA_DM_FIFO:
			sentlen = fifo_write(d->ip, (char *) smp->data, len);
			return sentlen / sizeof(smp->data[0]);
			break;
	}
	
	return -1;
}

static struct node_type vt = {
	.name		= "fpga",
	.description	= "VILLASfpga PCIe card (libpci)",
	.size		= sizeof(struct fpga_dm),
	.vectorize	= 1,
	.parse		= fpga_parse,
	.print		= fpga_print,
	.open		= fpga_open,
	.close		= fpga_close,
	.read		= fpga_read,
	.write		= fpga_write,
	.init		= fpga_init,
	.deinit		= fpga_deinit
};

REGISTER_NODE_TYPE(&vt)
