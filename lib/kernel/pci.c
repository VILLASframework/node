/** Linux PCI helpers
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <libgen.h> 
#include <unistd.h>

#include "log.h"

#include "kernel/pci.h"
#include "config.h"

struct pci_dev * pci_find_device(struct pci_access *pacc, struct pci_filter *f)
{
	struct pci_dev *d;

	/* Iterate over all devices */
	for (d = pacc->devices; d; d = d->next) {
		if (pci_filter_match(f, d))
			return d;
	}

	return NULL;
}

int pci_attach_driver(struct pci_dev *d, const char *driver)
{
	FILE *f;
	char fn[256];

	/* Add new ID to driver */
	snprintf(fn, sizeof(fn), "%s/bus/pci/drivers/%s/new_id", SYSFS_PATH, driver);
	f = fopen(fn, "w");
	if (!f)
		serror("Failed to add PCI id to %s driver (%s)", driver, fn);

	debug(5, "Adding ID to %s module: %04x %04x", driver, d->vendor_id, d->device_id);
	fprintf(f, "%04x %04x", d->vendor_id, d->device_id);
	fclose(f);

	/* Bind to driver */
	snprintf(fn, sizeof(fn), "%s/bus/pci/drivers/%s/bind", SYSFS_PATH, driver);
	f = fopen(fn, "w");
	if (!f)
		serror("Failed to bind PCI device to %s driver (%s)", driver, fn);

	debug(5, "Bind device to %s driver", driver);
	fprintf(f, "%04x:%02x:%02x.%x\n", d->domain, d->bus, d->dev, d->func);
	fclose(f);

	return 0;
}

int pci_get_iommu_group(struct pci_dev *pdev)
{
	int ret;
	char *group, link[1024], sysfs[1024];

	snprintf(sysfs, sizeof(sysfs), "%s/bus/pci/devices/%04x:%02x:%02x.%x/iommu_group", SYSFS_PATH,
		pdev->domain, pdev->bus, pdev->dev, pdev->func);

	ret = readlink(sysfs, link, sizeof(link));
	if (ret < 0)
		return -1;

	group = basename(link);

	return atoi(group);
}
