/** Node type: GTFPGA (Xilinx ML507)
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

#include "gtfpga.h"
#include "config.h"
#include "utils.h"
#include "timing.h"
#include "utils.h"
#include "kernel.h"

static struct pci_access *pacc;

static void gtfpga_debug(char *msg, ...) {
	va_list ap;

	va_start(ap, msg);
	log_vprint(LOG_LVL_DEBUG, msg, ap);
	va_end(ap);
}

int gtfpga_init(int argc, char * argv[], config_setting_t *cfg)
{
	if (getuid() != 0)
		error("The gtfpga node-type requires superuser privileges!");

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

int gtfpga_parse(struct node *n, config_setting_t *cfg)
{
	struct gtfpga *g = n->_vd;

	const char *slot, *id, *err;
	config_setting_t *cfg_slot, *cfg_id;

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

	return 0;
}

char * gtfpga_print(struct node *n)
{
	struct gtfpga *g = n->_vd;

	if (g->dev) {
		return strf("rate=%.1f slot=%04"PRIx16":%02"PRIx8":%02"PRIx8".%"PRIx8
			" id=%04"PRIx16":%04"PRIx16" class=%04"PRIx16" irq=%d (%s)", g->rate,
			g->dev->domain, g->dev->bus, g->dev->dev, g->dev->func, g->dev->vendor_id, g->dev->device_id,
			g->dev->device_class, g->dev->irq, g->name);
	}
	else {
		return strf("rate=%.1f slot=%02"PRIx8":%02"PRIx8".%"PRIx8" id=%04"PRIx16":%04"PRIx16, g->rate,
			g->filter.bus, g->filter.device, g->filter.func,
			g->filter.vendor, g->filter.device);
	}
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

int gtfpga_open(struct node *n)
{
	struct gtfpga *g = n->_vd;
	struct pci_dev *dev;

	dev = gtfpga_find_device(&g->filter);
	if (!dev)
		error("No GTFPGA card found");

	g->dev = dev;

	/* @todo VFIO */

	return 0;
}

int gtfpga_close(struct node *n)
{
	struct gtfpga *g = n->_vd;

	/* @todo VFIO */

	return 0;
}

int gtfpga_read(struct node *n, struct pool *pool, int cnt)
{
	struct gtfpga *g = n->_vd;

	/* Wait for IRQ */
	uint64_t fired;
	read(g->fd_irq, &fired, sizeof(fired));

	/* Copy memory mapped data */
	/** @todo */

	return 0;
}

/** @todo implement */
int gtfpga_write(struct node *n, struct pool *pool, int cnt)
{
	struct gtfpga *g = n->_vd;

	/* Copy memory mapped data */
	/** @todo */

	return 0;
}

static struct node_type vt = {
	.name		= "gtfpga",
	.description	= "GTFPGA PCIe card (libpci)",
	.vectorize	= 0,
	.parse		= gtfpga_parse,
	.print		= gtfpga_print,
	.open		= gtfpga_open,
	.close		= gtfpga_close,
	.read		= gtfpga_read,
	.write		= gtfpga_write,
	.init		= gtfpga_init,
	.deinit		= gtfpga_deinit
};

REGISTER_NODE_TYPE(&vt)