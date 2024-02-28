/** Linux PCI helpers
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

/** @addtogroup fpga Kernel @{ */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <villas/list.h>

#define PCI_SLOT(devfn)		(((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn)		((devfn) & 0x07)

struct pci_device {
	struct {
		int vendor;
		int device;
		int class_code;
	} id;

	struct {
		int domain;
		int bus;
		int device;
		int function;
	} slot;			/**< Bus, Device, Function (BDF) */
};

struct pci_region {
	int num;
	uintptr_t start;
	uintptr_t end;
	unsigned long long flags;
};

struct pci {
	struct vlist devices; /**< List of available PCI devices in the system (struct pci_device) */
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

int pci_device_parse_slot(struct pci_device *f, const char *str, const char **error);

int pci_device_parse_id(struct pci_device *f, const char *str, const char **error);

int pci_device_compare(const struct pci_device *d, const struct pci_device *f);

struct pci_device * pci_lookup_device(struct pci *p, struct pci_device *filter);

/** Get currently loaded driver for device */
int pci_get_driver(const struct pci_device *d, char *buf, size_t buflen);

/** Bind a new LKM to the PCI device */
int pci_attach_driver(const struct pci_device *d, const char *driver);

/** Return the IOMMU group of this PCI device or -1 if the device is not in a group. */
int pci_get_iommu_group(const struct pci_device *d);

size_t pci_get_regions(const struct pci_device *d, struct pci_region** regions);

/** @} */
