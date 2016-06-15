/** Node type: VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "kernel/kernel.h"
#include "kernel/pci.h"

#include "nodes/vfpga.h"

#include "fpga/ip.h"

#include "config-fpga.h"
#include "utils.h"
#include "timing.h"

struct vfpga fpga;
struct vfio_container vc;
static struct pci_access *pacc;

typedef void(*log_cb_t)(char *, ...);

int vfpga_reset(struct vfpga *f)
{
	int ret;
	char state[4096];

	/* Save current state of PCI configuration space */
	ret = pread(f->vd.fd, state, sizeof(state), (off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40);
	if (ret != sizeof(state))
		return -1;

	uint32_t *rst_reg = (uint32_t *) (f->map + f->baseaddr.reset);

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

void vfpga_dump(struct vfpga *f)
{
	char namebuf[128];
	char *name;

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

struct vfpga * vfpga_get()
{
	return &fpga;
}

static void vfpga_debug(char *msg, ...) {
	va_list ap;

	va_start(ap, msg);
	log_vprint(LOG_LVL_DEBUG, msg, ap);
	va_end(ap);
}

int vfpga_parse(struct vfpga *v, int argc, char * argv[], config_setting_t *cfg)
{
	int ret;
	const char *slot, *id, *err;
	config_setting_t *cfg_fpga, *cfg_ips, *cfg_slot, *cfg_id;

	/* Default values */
	v->filter.vendor = PCI_VID_XILINX;
	v->filter.device = PCI_PID_VFPGA;
	v->reset = false;

	cfg_fpga = config_setting_get_member(cfg, "fpga");
	if (!cfg_fpga)
		cerror(cfg, "Config file is missing VILLASfpga configuration");

	config_setting_lookup_bool(cfg_fpga, "reset", &v->reset);

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
		struct ip ip;
		config_setting_t *cfg_ip = config_setting_get_elem(cfg_ips, i);

		ret = ip_parse(&ip, cfg_ip);
		if (ret)
			cerror(cfg_ip, "Failed to parse VILLASfpga ip");

		list_push(&v->ips, memdup(&ip, sizeof(struct ip)));
	}

	return 0;
}

struct pci_access * vfpga_init_pci()
{
	struct pci_access *pacc;

	pacc = pci_alloc();		/* Get the pci_access structure */
	if (!pacc)
		error("Failed to allocate PCI access structure");

	pci_init(pacc);			/* Initialize the PCI library */
	pci_scan_bus(pacc);		/* We want to get the list of devices */

	pacc->error = (log_cb_t) error;	/* Replace logging and debug functions */
	pacc->warning = (log_cb_t) warn;
	pacc->debug = vfpga_debug;

	pci_scan_bus(pacc);		/* We want to get the list of devices */

	return pacc;
}

int vfpga_init(int argc, char * argv[], config_setting_t *cfg)
{
	int ret;
	struct pci_access *pacc;
	struct pci_dev *pdev;
	struct vfpga *f;

	/* For now we only support a single VILALSfpga card */
	f = vfpga_get();

	/* Some hardcoded addresses for internal IP blocks */
	f->baseaddr.reset = 0x2000;
	f->baseaddr.intc  = 0x5000;

	pacc = vfpga_init_pci();

	pci_filter_init(pacc, &f->filter);
	list_init(&f->ips);

	ret = vfpga_parse(f, argc, argv, cfg);
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

	/* Setup IRQs */
	for (int i = 0; i < f->vd.irqs[VFIO_PCI_MSI_IRQ_INDEX].count; i++) {
		ret = vfio_pci_msi_fd(&f->vd, (1 << i));
		if (ret < 0)
			serror("Failed to create eventfd for IRQ: ret=%d", f->vd.msi_efds[i]);
	}

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
	
	/* Trigger internal reset of fpga card / PCIe endpoint */
	if (f->reset) {
		ret = vfpga_reset(f);
		if (ret)
			serror("Failed to reset fpga card");
		
		/* Reset / detect PCI device */
		ret = vfio_pci_reset(&f->vd);
		if (ret)
			serror("Failed to reset PCI device");
	}

	list_foreach(struct ip *c, &f->ips) {
		c->card = f;
		ip_init(c);
	}

	return 0;
}

int vfpga_deinit()
{
	int ret;

	list_destroy(&fpga.ips, (dtor_cb_t) ip_destroy, true);

	pci_cleanup(pacc);

	ret = vfio_destroy(&vc);
	if (ret)
		error("Failed to deinitialize VFIO module");

	return 0;
}

int vfpga_parse_node(struct node *n, config_setting_t *cfg)
{
//	struct vfpga *v = n->_vd;

	return 0;
}

char * vfpga_print(struct node *n)
{
	struct vfpga *v = n->_vd;

	return strf("slot=%02"PRIx8":%02"PRIx8".%"PRIx8" id=%04"PRIx16":%04"PRIx16,
		v->filter.bus, v->filter.device, v->filter.func,
		v->filter.vendor, v->filter.device);
}

int vfpga_open(struct node *n)
{
	struct vfpga *v = n->_vd;
	struct pci_dev *dev;

	dev = pci_find_device(pacc, &v->filter);
	if (!dev)
		error("No vfpga card found");

	/* @todo */

	return 0;
}

int vfpga_close(struct node *n)
{
//	struct vfpga *v = n->_vd;

	/* @todo */

	return 0;
}

int vfpga_read(struct node *n, struct sample *smps[], unsigned cnt)
{
//	struct vfpga *v = n->_vd;

	/** @todo */

	return 0;
}

/** @todo implement */
int vfpga_write(struct node *n, struct sample *smps[], unsigned cnt)
{
//	struct vfpga *v = n->_vd;

	/** @todo */

	return 0;
}

static struct node_type vt = {
	.name		= "vfpga",
	.description	= "VILLASfpga PCIe card (libpci)",
	.vectorize	= 1,
	.parse		= vfpga_parse_node,
	.print		= vfpga_print,
	.open		= vfpga_open,
	.close		= vfpga_close,
	.read		= vfpga_read,
	.write		= vfpga_write,
	.init		= vfpga_init,
	.deinit		= vfpga_deinit
};

REGISTER_NODE_TYPE(&vt)