/** Node type: GTFPGA (Xilinx ML507)
 *
 * This file implements the gtfpga subtype for nodes.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2015, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of S2SS. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <stdio.h>
#include <inttypes.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "gtfpga.h"
#include "utils.h"
#include "timing.h"
#include "checks.h"

static struct pci_access *pacc;

static void gtfpga_debug(char *msg, ...) {
	va_list ap;

	va_start(ap, msg);
	log_vprint(DEBUG, msg, ap);
	va_end(ap);
}

int gtfpga_init(int argc, char * argv[], struct settings *set)
{
	pacc = pci_alloc();		/* Get the pci_access structure */
	if (!pacc)
		error("Failed to allocate PCI access structure");

	pci_init(pacc);			/* Initialize the PCI library */

	pacc->error = (log_cb_t) error;	/* Replace logging and debug functions */
	pacc->warning = (log_cb_t) warn;
	pacc->debug = gtfpga_debug;

	pci_scan_bus(pacc);		/* We want to get the list of devices */

	return 0;
}

int gtfpga_deinit()
{
	pci_cleanup(pacc);

	return 0;
}

int gtfpga_parse(config_setting_t *cfg, struct node *n)
{
	struct gtfpga *g = alloc(sizeof(struct gtfpga));

	const char *slot, *id, *err;
	config_setting_t *cfg_slot, *cfg_id;

	/* Checks */
	if (n->combine != 1) {
		config_setting_t *cfg_combine = config_setting_get_member(cfg, "combine");
		cerror(cfg_combine, "The GTFPGA node type does not support combining!");
	}

	pci_filter_init(NULL, &g->filter);

	cfg_slot = config_setting_get_member(cfg, "slot");
	if (cfg_slot) {
		slot = config_setting_get_string(cfg_slot);
		if (slot) {
			err = pci_filter_parse_slot(&g->filter, (char*) slot);
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
			err = pci_filter_parse_id(&g->filter, (char*) id);
			if (err)
				cerror(cfg_id, "%s", err);
		}
		else
			cerror(cfg_slot, "Invalid id format");
	}

	if (!config_setting_lookup_float(cfg, "rate", &g->rate))
		g->rate = 0;

	n->gtfpga = g;

	return 0;
}

int gtfpga_print(struct node *n, char *buf, int len)
{
	struct gtfpga *g = n->gtfpga;

	if (g->dev) {
		return snprintf(buf, len, "rate=%.1f slot=%04"PRIx16":%02"PRIx8":%02"PRIx8".%"PRIx8
			" id=%04"PRIx16":%04"PRIx16" class=%04"PRIx16" irq=%d (%s)", g->rate,
			g->dev->domain, g->dev->bus, g->dev->dev, g->dev->func, g->dev->vendor_id, g->dev->device_id,
			g->dev->device_class, g->dev->irq, g->name);
	}
	else {
		return snprintf(buf, len, "rate=%.1f slot=%02"PRIx8":%02"PRIx8".%"PRIx8" id=%04"PRIx16":%04"PRIx16, g->rate,
			g->filter.bus, g->filter.device, g->filter.func,
			g->filter.vendor, g->filter.device);
	}
}

static int gtfpga_load_driver(struct pci_dev *d)
{
	FILE *f;
	char slot[16];

	if (check_kernel_module("uio_pci_generic"))
		error("Missing kernel module: uio_pci_generic");

	/* Prepare slot identifier */
	snprintf(slot, sizeof(slot), "%04x:%02x:%02x.%x",
		d->domain, d->bus, d->dev, d->func);

	/* Add new ID to uio_pci_generic */
	if (!(f = fopen(SYSFS_PATH "/bus/pci/drivers/uio_pci_generic/new_id", "w")))
		serror("Failed to add PCI id to uio_pci_generic driver");

	debug(5, "Adding ID to uio_pci_generic module: %04x %04x", d->vendor_id, d->device_id);
	fprintf(f, "%04x %04x", d->vendor_id, d->device_id);
	fclose(f);

	/* Bind to uio_pci_generic */
	f = fopen(SYSFS_PATH "/drivers/uio_pci_generic/bind", "w");
	if (!f)
		serror("Failed to add PCI id to uio_pci_generic driver");

	debug(5, "Bind slot to uio_pci_generic module: %s", slot);
	fprintf(f, "%s\n", slot);
	fclose(f);

	return 0;
}

static struct pci_dev * gtfpga_find_device(struct pci_filter *f)
{
	struct pci_dev *d;

	/* Iterate over all devices */
	for (d = pacc->devices; d; d = d->next) {
		if (pci_filter_match(f, d))
			return d;
	}

	return NULL;
}

static int gtfpga_mmap(struct gtfpga *g)
{
	int fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (!fd)
		serror("Failed open()");

	long int addr = g->dev->base_addr[GTFPGA_BAR] & ~0xfff;
	int size = g->dev->size[GTFPGA_BAR];

	/* mmap() first BAR */
	debug(5, "Setup mapping: mmap(NULL, %#x, PROT_READ | PROT_WRITE, MAP_SHARED, %u, %#lx)", size, fd, addr);
	void *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr);
	if (map == MAP_FAILED)
		serror("Failed mmap()");

	return 0;
}

int gtfpga_open(struct node *n)
{
	struct gtfpga *g = n->gtfpga;
	struct pci_dev *dev;

	int ret;

	dev = gtfpga_find_device(&g->filter);
	if (!dev)
		error("No GTFPGA card found");

	g->dev = dev;
	g->name = alloc(512);

	gtfpga_load_driver(dev);
	gtfpga_mmap(g);

	/* Show some debug infos */
	pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_CLASS);	/* Fill in header info we need */

	g->name = pci_lookup_name(pacc, g->name, 512, PCI_LOOKUP_DEVICE, dev->vendor_id, dev->device_id);

	/* Setup timer */
	if (g->rate) {
		g->fd_irq = timerfd_create(CLOCK_MONOTONIC, 0);
		if (g->fd_irq < 0)
			serror("Failed to create timer");

		struct itimerspec its = {
			.it_interval = time_from_double(1 / g->rate),
			.it_value = { 1, 0 }
		};
		ret = timerfd_settime(g->fd_irq, 0, &its, NULL);
		if (ret)
			serror("Failed to start timer");
	}
	else
		/** @todo implement UIO interrupts */
		error("UIO irq not implemented yet. Use 'rate' setting");

	char buf[1024];
	gtfpga_print(n, buf, sizeof(buf));
	debug(5, "Found GTFPGA card: %s", buf);

	return 0;
}

int gtfpga_close(struct node *n)
{
	struct gtfpga *g = n->gtfpga;

	if (g->map)
		munmap(g->map, g->dev->size[GTFPGA_BAR]);

	close(g->fd_mmap);
	close(g->fd_irq);
	free(g->name);

	return 0;
}

/** @todo implement */
int gtfpga_read(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	struct gtfpga *g = n->gtfpga;

	struct msg *m = &pool[first % poolsize];

	uint64_t runs;
	read(g->fd_irq, &runs, sizeof(runs));

	static int seq;

	m->sequence = seq++;

	return 0;
}

/** @todo implement */
int gtfpga_write(struct node *n, struct msg *pool, int poolsize, int first, int cnt)
{
	// struct gtfpga *g = n->gtfpga;

	// struct msg *m = &pool[first % poolsize];

	return 0;
}

