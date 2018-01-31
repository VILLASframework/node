/** Virtual Function IO wrapper around kernel API
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 *********************************************************************************/

/** @addtogroup fpga Kernel @{ */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>

#include <linux/vfio.h>
#include <linux/pci_regs.h>

#include "list.h"

#define VFIO_DEV(x)	"/dev/vfio/" x

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
struct pci_device;

struct vfio_group {
	int fd;					/**< VFIO group file descriptor */
	int index;				/**< Index of the IOMMU group as listed under /sys/kernel/iommu_groups/ */

	struct vfio_group_status status;	/**< Status of group */

	struct list devices;

	struct vfio_container *container;	/**< The VFIO container to which this group is belonging */
};

struct vfio_device {
	char *name;				/**< Name of the device as listed under /sys/kernel/iommu_groups/[vfio_group::index]/devices/ */
	int fd;					/**< VFIO device file descriptor */

	struct vfio_device_info info;
	struct vfio_irq_info *irqs;
	struct vfio_region_info *regions;

	void **mappings;

	struct pci_device *pci_device;		/**< libpci handle of the device */
	struct vfio_group *group;		/**< The VFIO group this device belongs to */
};

struct vfio_container {
	int fd;
	int version;
	int extensions;

	uint64_t iova_next;			/**< Next free IOVA address */

	struct list groups;
};

/** Initialize a new VFIO container. */
int vfio_init(struct vfio_container *c);

/** Initialize a VFIO group and attach it to an existing VFIO container. */
int vfio_group_attach(struct vfio_group *g, struct vfio_container *c, int index);

/** Initialize a VFIO device, lookup the VFIO group it belongs to, create the group if not already existing. */
int vfio_device_attach(struct vfio_device *d, struct vfio_container *c, const char *name, int index);

/** Initialie a VFIO-PCI device (uses vfio_device_attach() internally) */
int vfio_pci_attach(struct vfio_device *d, struct vfio_container *c, struct pci_device *pdev);

/** Hot resets a VFIO-PCI device */
int vfio_pci_reset(struct vfio_device *d);

int vfio_pci_msi_init(struct vfio_device *d, int efds[32]);

int vfio_pci_msi_deinit(struct vfio_device *d, int efds[32]);

int vfio_pci_msi_find(struct vfio_device *d, int nos[32]);

/** Enable memory accesses and bus mastering for PCI device */
int vfio_pci_enable(struct vfio_device *d);

/** Reset a VFIO device */
int vfio_device_reset(struct vfio_device *d);

/** Release memory and close container */
int vfio_destroy(struct vfio_container *c);

/** Release memory of group */
int vfio_group_destroy(struct vfio_group *g);

/** Release memory of device */
int vfio_device_destroy(struct vfio_device *g);

/** Print a dump of all attached groups and their devices including regions and IRQs */
void vfio_dump(struct vfio_container *c);

/** Map a device memory region to the application address space (e.g. PCI BARs) */
void * vfio_map_region(struct vfio_device *d, int idx);

/** Map VM to an IOVA, which is accessible by devices in the container */
int vfio_map_dma(struct vfio_container *c, uint64_t virt, uint64_t phys, size_t len);

/** Unmap DMA memory */
int vfio_unmap_dma(struct vfio_container *c, uint64_t virt, uint64_t phys, size_t len);

/** munmap() a region which has been mapped by vfio_map_region() */
int vfio_unmap_region(struct vfio_device *d, int idx);

#ifdef __cplusplus
}
#endif

/** @} */
