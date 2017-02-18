/** Linux PCI helpers
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 **********************************************************************************/

#pragma once

#include "list.h"

#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)		((devfn) & 0x07)

struct pci_dev {
	struct {
		int vendor;
		int device;
		int class;
	} id;
	
	struct {
		int domain;
		int bus;
		int device;
		int function;
	} slot;			/**< Bus, Device, Function (BDF) */
};

struct pci {
	struct list devices; /**> List of available PCI devices in the system (struct pci_dev) */
};

/** Initialize Linux PCI handle.
 *
 * This search for all available PCI devices under /sys/bus/pci
 *
 * @retval 0 Success. Everything went well.
 * @retval <0 Error. Something went wrong.
 */
int pci_init(struct pci *p);

/** Destroy handle. */
int pci_destroy(struct pci *p);

int pci_dev_parse_slot(struct pci_dev *f, const char *str, const char **error);

int pci_dev_parse_id(struct pci_dev *f, const char *str, const char **error);

int pci_dev_compare(const struct pci_dev *d, const struct pci_dev *f);

struct pci_dev * pci_lookup_device(struct pci *p, struct pci_dev *filter);

/** Bind a new LKM to the PCI device */
int pci_attach_driver(struct pci_dev *d, const char *driver);

/** Return the IOMMU group of this PCI device or -1 if the device is not in a group. */
int pci_get_iommu_group(struct pci_dev *d);