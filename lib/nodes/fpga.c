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

#include "fpga/ip.h"

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

	info("Reset fpga");
	rst_reg[0] = 1;

	usleep(100000);

	/* Restore previous state of PCI configuration space */
	ret = pwrite(f->vd.fd, state, sizeof(state), (off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40);
	if (ret != sizeof(state))
		return -1;

	info("Reset status = %#x", *rst_reg);

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
		info("DMA mapped at %p", fpga.dma);

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

int fpga_parse_card(struct fpga *v, int argc, char * argv[], config_setting_t *cfg)
{
	int ret;
	const char *slot, *id, *err;
	config_setting_t *cfg_fpga, *cfg_ips, *cfg_slot, *cfg_id;

	/* Default values */
	v->filter.vendor = PCI_VID_XILINX;
	v->filter.device = PCI_PID_VFPGA;

	cfg_fpga = config_setting_get_member(cfg, "fpga");
	if (!cfg_fpga)
		cerror(cfg, "Config file is missing VILLASfpga configuration");

	config_setting_lookup_bool(cfg_fpga, "do_reset", &v->do_reset);

	cfg_slot = config_setting_get_member(cfg_fpga, "slot");
	if (cfg_slot) {
		slot = config_setting_get_string(cfg_slot);
		if (slot) {
			err = pci_filter_parse_slot(&v->filter, (char*) slot);
			if (err)
				cerror(cfg_slot, "%s", err);
		}
		else
			cerror(cfg_slot, "Invalid slot format");
	}

	cfg_id = config_setting_get_member(cfg_fpga, "id");
	if (cfg_id) {
		id = config_setting_get_string(cfg_id);
		if (id) {
			err = pci_filter_parse_id(&v->filter, (char*) id);
			if (err)
				cerror(cfg_id, "%s", err);
		}
		else
			cerror(cfg_slot, "Invalid id format");
	}
	
	cfg_ips = config_setting_get_member(cfg_fpga, "ips");
	if (!cfg_ips)
		cerror(cfg_fpga, "FPGA configuration is missing ips section");

	for (int i = 0; i < config_setting_length(cfg_ips); i++) {
		struct ip *ip;
		config_setting_t *cfg_ip = config_setting_get_elem(cfg_ips, i);

		ip = alloc(sizeof(struct ip));
		ret = ip_parse(ip, cfg_ip);
		if (ret) {
			free(ip);
			cerror(cfg_ip, "Failed to parse VILLASfpga ip");
		}

		list_push(&v->ips, ip);
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
	
	/* Check FPGA configuration */
	f->reset = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_gpio", NULL);
	if (!reset)
		error("FPGA is missing a reset controller");

	f->intc = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axi_pcie_intc", NULL);
	if (!f->intc)
		error("FPGA is missing a interrupt controller");

	f->sw = ip_vlnv_lookup(&f->ips, "xilinx.com", "ip", "axis_interconnect", NULL);
	if (!f->sw)
		warn("FPGA is missing an AXI4-Stream switch");

	pacc = pci_get_handle();
	pci_filter_init(pacc, &f->filter);
	list_init(&f->ips);

	ret = fpga_parse_card(f, argc, argv, cfg);
	if (ret)
		cerror(cfg, "Failed to parse VILLASfpga config");

	/* Search for fpga card */
	pdev = pci_find_device(pacc, &f->filter);
	if (!pdev)
		error("Failed to find PCI device");

	if (pdev->vendor_id != PCI_VID_XILINX)
		error("This is not a Xilinx FPGA board!");

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
	
	/* Map DMA accessible memory */
	f->dma = vfio_map_dma(f->vd.group->container, 0x10000, 0x1000, 0x0);
	if (f->dma == MAP_FAILED)
		serror("Failed to mmap() DMA");

	/* Enable memory access and PCI bus mastering for DMA */
	ret = vfio_pci_enable(&f->vd);
	if (ret)
		serror("Failed to enable PCI device");
	
	/* Reset system ? */
	if (f->do_reset) {
		ret = fpga_reset(f);
		if (ret)
			serror("Failed to reset fpga card");
		
		/* Reset / detect PCI device */
		ret = vfio_pci_reset(&f->vd);
		if (ret)
			serror("Failed to reset PCI device");
	}

	/* Initialize IP cores */
	list_foreach(struct ip *c, &f->ips) {
		c->card = f;
		ip_init(c);
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

	const char *dm;

	if (!config_setting_lookup_string(cfg, "datamover", &dm))
		cerror(cfg, "Node '%s' is missing the 'datamover' setting", node_name(n));

	d->ip = list_lookup(&d->card->ips, dm);
	if (!d->ip)
		cerror(cfg, "Datamover '%s' is unknown. Please specify it in the fpga.ips section", dm);

	if      (ip_vlnv_match(d->ip, "xilinx.com", "ip", "axi_dma", NULL))
		d->type = FPGA_DM_DMA;
	else if (ip_vlnv_match(d->ip, "xilinx.com", "ip", "axi_fifo_mm", NULL))
		d->type = FPGA_DM_FIFO;
	else
		cerror(cfg, "IP '%s' is not a supported datamover", dm);

	return 0;
}

char * fpga_print(struct node *n)
{
	struct fpga_dm *d = n->_vd;
	struct fpga *f = d->card;

	return strf("dm=%s (%s:%s:%s:%s) baseaddr=%#jx port=%u slot=%02"PRIx8":%02"PRIx8".%"PRIx8" id=%04"PRIx16":%04"PRIx16,
		d->ip->name, d->ip->vlnv.vendor, d->ip->vlnv.library, d->ip->vlnv.name, d->ip->vlnv.version, d->ip->baseaddr, d->ip->port,
		f->filter.bus, f->filter.device, f->filter.func,
		f->filter.vendor, f->filter.device);
}

int fpga_open(struct node *n)
{
//	struct fpga *v = n->_vd;

	/** @todo Implement */

	return 0;
}

int fpga_close(struct node *n)
{
//	struct fpga *v = n->_vd;

	/** @todo Implement */

	return 0;
}

int fpga_read(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct fpga_dm *d = n->_vd;
	struct sample *smp = smps[0];

	char *addr = (char *) smp->values;
	size_t len = smp->length * sizeof(smp->values[0]);

	/* Read data from RTDS */
	switch (d->type) {
		case FPGA_DM_DMA:
			if (d->ip->dma.HasSg)
				return -1; /* Currently not supported */
			else {
				len = dma_read(d->ip, addr, len);
				XAxiDma_IntrAckIrq(&d->ip->dma, XAXIDMA_IRQ_IOC_MASK, XAXIDMA_DEVICE_TO_DMA);
				return 1;
			}
		case FPGA_DM_FIFO:
			break;
	}

	return -1;
}

int fpga_write(struct node *n, struct sample *smps[], unsigned cnt)
{
	struct fpga_dm *d = n->_vd;
	struct sample *smp = smps[0];

	char *addr = (char *) smp->values;
	size_t len = smp->length * sizeof(smp->values[0]);

	/* Send data to RTDS */
	switch (d->type) {
		case FPGA_DM_DMA:
			len = dma_write(d->ip, addr, len);
			return 1;
		case FPGA_DM_FIFO:
			break;
	}
	
	return -1;
}

static struct node_type vt = {
	.name		= "fpga",
	.description	= "VILLASfpga PCIe card (libpci)",
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