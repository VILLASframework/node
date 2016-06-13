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

#include "nodes/vfpga.h"

#include "config.h"
#include "config-fpga.h"
#include "utils.h"
#include "timing.h"

static struct pci_access *pacc;

typedef void(*log_cb_t)(char *, ...);

int vfpga_init2(struct vfpga *f, struct vfio_container *vc, struct pci_dev *pdev)
{
	int ret;

	list_init(&f->models);
	
	f->features = 0;

	if (pdev->vendor_id != PCI_VID_XILINX)
		error("This is not a Xilinx FPGA board!");

	/* Attach PCIe card to VFIO container */
	ret = vfio_pci_attach(&f->vd, vc, pdev);
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
	
#if 1
	/* Trigger internal reset of fpga card / PCIe endpoint */
	ret = vfpga_reset(f);
	if (ret)
		serror("Failed to reset fpga card");
#endif

	/* Reset / detect PCI device */
	ret = vfio_pci_reset(&f->vd);
	if (ret)
		serror("Failed to reset PCI device");

	/* Initialize Interrupt controller */
	XIntc_Out32((uintptr_t) f->map + BASEADDR_INTC + XIN_IMR_OFFSET, 0);
	XIntc_Out32((uintptr_t) f->map + BASEADDR_INTC + XIN_IAR_OFFSET, 0xFFFFFFFF); /* Acknowlege all pending IRQs */
	usleep(1000);

	/* Setup Vectors */
	for (int i = 0; i < 16; i++)
		XIntc_Out32((uintptr_t) f->map + BASEADDR_INTC + XIN_IVAR_OFFSET + i * sizeof(uint32_t), i);

	XIntc_Out32((uintptr_t) f->map + BASEADDR_INTC + XIN_IMR_OFFSET, 0xFFFFFFFF); /* Use fast acknowlegement for all IRQs */
	XIntc_Out32((uintptr_t) f->map + BASEADDR_INTC + XIN_IER_OFFSET, 0x00000000);
	XIntc_Out32((uintptr_t) f->map + BASEADDR_INTC + XIN_MER_OFFSET, XIN_INT_HARDWARE_ENABLE_MASK | XIN_INT_MASTER_ENABLE_MASK);

#ifdef HAS_SWITCH
	/* Initialize AXI4-Sream crossbar switch */
	ret = switch_init(&f->sw, f->map + BASEADDR_SWITCH, AXI_SWITCH_NUM, AXI_SWITCH_NUM);
	if (ret)
		error("Failed to initialize switch");
#endif

#ifdef HAS_XSG
	/* Initialize System Generator model */
	struct model *m = alloc(sizeof(struct model));
	m->baseaddr = f->map + BASEADDR_XSG;
	m->type = MODEL_TYPE_XSG;

	ret = model_init(m, NULL);
	if (ret)
		error("Failed to init model: %d", ret);

	ret = xsg_init_from_map(m);
	if (ret)
		error("Failed to init XSG model: %d", ret);
	
	list_push(&f->models, m);
#endif

#ifdef HAS_FIFO_MM
	/* Initialize AXI4-Stream Memory Mapped FIFO */
	ret = fifo_init(&f->fifo, f->map + BASEADDR_FIFO_MM, f->map + BASEADDR_FIFO_MM_AXI4);
	if (ret)
		error("Failed to initialize FIFO");
#endif

#ifdef HAS_DMA_SG
	/* Initialize Scatter Gather DMA controller */
	struct dma_mem bd = {
		.base_virt = (uintptr_t) f->map + BASEADDR_BRAM,
		.base_phys = BASEADDR_BRAM,
		.len = 0x2000
	};
	struct dma_mem rx = {
		.base_virt = (uintptr_t) f->dma,
		.base_phys = BASEADDR_HOST,
		.len = 0x10000
	};

	ret = dma_init(&f->dma_sg, f->map + BASEADDR_DMA_SG, DMA_MODE_SG, &bd, &rx, sizeof(uint32_t) * 64);
	if (ret)
		return -1;
#endif

#ifdef HAS_DMA_SIMPLE
	/* Initialize Simple DMA controller */
	ret = dma_init(&f->dma_simple, f->map + BASEADDR_DMA_SIMPLE, DMA_MODE_SIMPLE, 0, 0, 0);
	if (ret)
		return -1;
#endif

#ifdef HAS_TIMER
	/* Initialize timer */
	XTmrCtr_Config tmr_cfg = {
		.BaseAddress = (uintptr_t) f->map + BASEADDR_TIMER,
		.SysClockFreqHz = AXI_HZ
	};

	XTmrCtr_CfgInitialize(&f->tmr, &tmr_cfg, (uintptr_t) f->map + BASEADDR_TIMER);
	XTmrCtr_InitHw(&f->tmr);
#endif

	return 0;
}

int vfpga_deinit2(struct vfpga *f)
{
	list_destroy(&f->models, (dtor_cb_t) model_destroy, true);

	return 0;
}

int vfpga_reset(struct vfpga *f)
{
	int ret;
	char state[4096];

	/* Save current state of PCI configuration space */
	ret = pread(f->vd.fd, state, sizeof(state), (off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40);
	if (ret != sizeof(state))
		return -1;

	uint32_t *rst_reg = (uint32_t *) (f->map + BASEADDR_RESET);

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

/* Dump some details about the fpga card */
void vfpga_dump(struct vfpga *f)
{
	char namebuf[128];
	char *name;

	name = pci_lookup_name(pacc, namebuf, sizeof(namebuf), PCI_LOOKUP_DEVICE, f->vd.pdev->vendor_id, f->vd.pdev->device_id);
	pci_fill_info(f->vd.pdev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);	/* Fill in header info we need */

	info("Found fpga card: %s", name);
	{ INDENT
		info("slot=%04x:%02x:%02x.%d", f->vd.pdev->domain, f->vd.pdev->bus, f->vd.pdev->dev, f->vd.pdev->func);
		info("vendor=%04x", f->vd.pdev->vendor_id);
		info("device=%04x", f->vd.pdev->device_id);
		info("class=%04x", f->vd.pdev->device_class);
	}
	info("BAR0 mapped at %p", f->map);

	vfio_dump(f->vd.group->container);
}

static void vfpga_debug(char *msg, ...) {
	va_list ap;

	va_start(ap, msg);
	log_vprint(LOG_LVL_DEBUG, msg, ap);
	va_end(ap);
}

int vfpga_init(int argc, char * argv[], config_setting_t *cfg)
{
	if (getuid() != 0)
		error("The vfpga node-type requires superuser privileges!");

	pacc = pci_alloc();		/* Get the pci_access structure */
	if (!pacc)
		error("Failed to allocate PCI access structure");

	pci_init(pacc);			/* Initialize the PCI library */

	pacc->error = (log_cb_t) error;	/* Replace logging and debug functions */
	pacc->warning = (log_cb_t) warn;
	pacc->debug = vfpga_debug;

	pci_scan_bus(pacc);		/* We want to get the list of devices */

	return 0;
}

int vfpga_deinit()
{
	pci_cleanup(pacc);

	return 0;
}

int vfpga_parse(struct node *n, config_setting_t *cfg)
{
	struct vfpga *v = n->_vd;

	const char *slot, *id, *err;
	config_setting_t *cfg_slot, *cfg_id;

	pci_filter_init(NULL, &v->filter);

	cfg_slot = config_setting_get_member(cfg, "slot");
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

	cfg_id = config_setting_get_member(cfg, "id");
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
	.vectorize	= 0,
	.parse		= vfpga_parse,
	.print		= vfpga_print,
	.open		= vfpga_open,
	.close		= vfpga_close,
	.read		= vfpga_read,
	.write		= vfpga_write,
	.init		= vfpga_init,
	.deinit		= vfpga_deinit
};

REGISTER_NODE_TYPE(&vt)