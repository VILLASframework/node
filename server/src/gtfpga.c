/** Node type: GTFPGA (Xilinx ML507)
 *
 * This file implements the gtfpga subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015, Institute for Automation of Complex Power Systems, EONERC
 */

#include "gtfpga.h"

#define SYSFS_PATH "/sys/bus/pci"

static pci_access *pacc;

int gtfpga_init(int argc, char *argv[])
{
	pacc = pci_alloc();		/* Get the pci_access structure */
	pci_init(pacc);			/* Initialize the PCI library */
	
	pacc->error = error;		/* Replace logging and debug functions */
	pacc->warning = warn;
	pacc->debug = debug;
	
	pci_scan_bus(pacc);		/* We want to get the list of devices */
}

int gtfpga_deinit()
{
	pci_cleanup(pacc);
}

int gtfpga_parse(config_setting_t *cfg, struct node *n)
{
	char *slot, *id;
	config_setting_t *cfg_slot, *cfg_id;
	struct gtfpga *g = alloc(sizeof(struct gtfpga));

	pci_filter_init(NULL, &g->filter);

	if (cfg_slot = config_setting_get_member(cfg, "slot")) {
		if (slot = config_setting_get_string(cfg_slot)) {
			if ((err = pci_filter_parse_slot(&g->filter, slot))
				cerror(cfg_slot, "%s", err);
		}
		else
			cerror(cfg_slot, "Invalid slot format");
	}

	if (cfg_id = config_setting_get_member(cfg, "id")) {
		if (id = config_setting_get_string(cfg_id)) {
			if ((err = pci_filter_parse_id(&g->filter, id))
				cerror(cfg_id, "%s", err);
		}
		else
			cerror(cfg_slot, "Invalid id format");
	}


	return 0;
}

int gtfpga_print(struct node *n, char *buf, int len)
{
	
}

static int gtfpga_load_driver(struct pci_dev *d)
{
	FILE *f;
	char slot[16];
	int ret;
	
	/* Prepare slot identifier */
	snprintf(slot, sizeof(slot), "%04x:%02x:%02x.%x",
		d->domain, d->bus, d->slot, d->func);
		
	/* Load uio_pci_generic module */
	ret = system2("modprobe uio_pci_generic");
	if (ret)
		serror("Failed to load module");
	
	/* Add new ID to uio_pci_generic */
	f = fopen(SYSFS_PATH "/drivers/uio_pci_generic/new_id", "w");
	if (!f)
		serror("Failed to add PCI id to uio_pci_generic driver");
	
	fprintf(f, "%04x %04x", d->vendor_id, d->device_id);
	fclose(f);
	
	/* Bind to uio_pci_generic */
	f = fopen(SYSFS_PATH "/drivers/uio_pci_generic/bind", "w");
	if (!f)
		serror("Failed to add PCI id to uio_pci_generic driver");
	
	fprintf(f, "%s\n", slot);
	fclose(f);
}

static struct pci_dev * gtfpga_find_device(struct pci_filter *f)
{
	struct pci_dev *d;

	/* Iterate over all devices */
	for (d = pacc->devices; d; d = d->next) {
		if (pci_filter_match(&f, d))
			return d;
	}

	return NULL;
}

static int gtfpga_mmap(struct node *n)
{
	struct gtfpga *g = n->gtfpga;

	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (!fd)
		serror("Failed open()");

	long int addr = g->dev->base_addr[GTFPGA_BAR] & ~0xfff;
	int size = g->dev->size[GTFPGA_BAR];

	/* mmap() first BAR */
	printf("mmap(NULL, %#x, PROT_READ | PROT_WRITE, MAP_SHARED, %u, %#lx)", size, fd, addr);
	void *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr);
	if (map == MAP_FAILED)
		serror("Failed mmap()");
}

int gtfpga_open(struct node *n)
{
	struct gtfpga *g = n->gtfpga;
	struct pci_dev *dev;

	int ret;

	dev = gtfpga_find_device(g->filter);
	if (!dev)
		error("No GTFPGA card detected");

	g->dev = dev;

	ret = gtfpga_load_driver(dev);
	if (ret)
		error("Failed to load and bind driver (uio_pci_generic)");

	ret = gtfpga_mmap(g);
	if (ret)
		error("Failed to setup memory mapping for GTFGPA card");

	/* Show some debug infos */
	pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);	/* Fill in header info we need */

	debug(3, "Found GTFPGA card: %04x:%02x:%02x.%d vendor=%04x device=%04x class=%04x irq=%d",
		dev->domain, dev->bus, dev->dev, dev->func, dev->vendor_id, dev->device_id,
		dev->device_class, dev->irq);

	debug(3, " (%s)\n", pci_lookup_name(pacc, namebuf, sizeof(namebuf), PCI_LOOKUP_DEVICE, dev->vendor_id, dev->device_id));

	return 0;
}

int gtfpga_close(struct node *n)
{
	struct gtfpga *g = n->gtfpga;

	if (g->map)
		munmap(g->map, g->dev->size[GTFPGA_BAR]);

	close(g->fd_mmap);
	close(g->fd_uio);

	return 0;
}

/** @todo implement */
int gtfpga_read(struct node *n, struct msg *m)
{
	struct gtfpga *g = n->gtfpga;

	return 0;
}

/** @todo implement */
int gtfpga_write(struct node *n, struct msg *m)
{
	struct gtfpga *g = n->gtfpga;

	return 0;
}

