/** Node type: GTFPGA (Xilinx ML507)
 *
 * This file implements the gtfpga subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014, Institute for Automation of Complex Power Systems, EONERC
 */

#include "gtfpga.h"

#define SYSFS_PATH "/sys/bus/pci"

static int gtfpga_load_driver(struct node *n)
{

}

static struct pci_dev * gtfpga_find_device(struct node *n)
{
	struct gtfpga *g = n->gtfpga;

	struct pci_dev *dev;

	pacc = pci_alloc();		/* Get the pci_access structure */

	pci_init(pacc);			/* Initialize the PCI library */
	pci_scan_bus(pacc);		/* We want to get the list of devices */

	/* Iterate over all devices */
	for (dev = pacc->devices; dev; dev = dev->next) {
		if (pci_filter_match(&filter, dev))
			return dev;
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

	munmap(g->map, g->dev->size[GTFPGA_BAR]);

	close(g->fd_mmap);
	close(g->fd_uio);

	return 0;
}

int gtfpga_read(struct node *n, struct msg *m)
{
	struct gtfpga *g = n->gtfpga;

	return 0;
}

int gtfpga_write(struct node *n, struct msg *m)
{
	struct gtfpga *g = n->gtfpga;

	return 0;
}

