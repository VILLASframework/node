/** Virtual Function IO wrapper around kernel API
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#ifndef _VFIO_H_
#define _VFIO_H_

#include <stdbool.h>
#include <pci/pci.h>

#include <linux/vfio.h>
#include <linux/pci_regs.h>

#include <villas/list.h>

#define VFIO_DEV(x)	"/dev/vfio/" x

struct vfio_group {
	int fd;						/**< VFIO group file descriptor */
	int index;					/**< Index of the IOMMU group as listed under /sys/kernel/iommu_groups/ */

	struct vfio_group_status status;		/**< Status of group */

	struct list devices;

	struct vfio_container *container;		/**< The VFIO container to which this group is belonging */
};

struct vfio_dev {
	char *name;				/**< Name of the device as listed under /sys/kernel/iommu_groups/[vfio_group::index]/devices/ */
	int fd;					/**< VFIO device file descriptor */

	struct vfio_device_info info;
	struct vfio_irq_info *irqs;
	struct vfio_region_info *regions;
	
	void **mappings;
	
	int *msi_efds;				/**< Array of assigned eventfs for MSI handling */
	int *msi_irqs;				/**< Linux assigned IRQ number for MSI (see /proc/interrupts) */

	struct pci_dev *pdev;			/**< libpci handle of the device */
	struct vfio_group *group;		/**< The VFIO group this device belongs to */
};

struct vfio_container {
	int fd;
	int version;
	int extensions;

	struct list groups;
};

/** Initialize a new VFIO container. */
int vfio_init(struct vfio_container *c);

/** Initialize a VFIO group and attach it to an existing VFIO container. */
int vfio_group_attach(struct vfio_group *g, struct vfio_container *c, int index);

/** Initialize a VFIO device, lookup the VFIO group it belongs to, create the group if not already existing. */
int vfio_dev_attach(struct vfio_dev *d, struct vfio_container *c, const char *name, int index);

/** Initialie a VFIO-PCI device (uses vfio_dev_attach() internally) */
int vfio_pci_attach(struct vfio_dev *d, struct vfio_container *c, struct pci_dev *pdev);

/** Hot resets a VFIO-PCI device */
int vfio_pci_reset(struct vfio_dev *d);

/** Create a new eventfd and binds it to the MSI irqs of the device */
int vfio_pci_msi_fd(struct vfio_dev *d, uint32_t mask);

/** Enable memory accesses and bus mastering for PCI device */
int vfio_pci_enable(struct vfio_dev *d);

/** Reset a VFIO device */
int vfio_dev_reset(struct vfio_dev *d);

/** Release memory and close container */
int vfio_destroy(struct vfio_container *c);

/** Release memory of group */
void vfio_group_destroy(struct vfio_group *g);

/** Release memory of device */
void vfio_dev_destroy(struct vfio_dev *g);

/** Print a dump of all attached groups and their devices including regions and IRQs */
void vfio_dump(struct vfio_container *c);

/** Map a device memory region to the application address space (e.g. PCI BARs) */
void * vfio_map_region(struct vfio_dev *d, int idx);

/** Allocate a virtual memory region and map it for DMA access from the devices */
void * vfio_map_dma(struct vfio_container *c, size_t size, size_t pgsize, uint64_t phyaddr);

/** munmap() a region which has been mapped by vfio_map_region() */
int vfio_unmap_region(struct vfio_dev *d, int idx);

#endif /* _VFIO_H_ */
