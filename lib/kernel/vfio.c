/** Virtual Function IO wrapper around kernel API
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 **********************************************************************************/

#define _DEFAULT_SOURCE

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include <sys/ioctl.h>
#include <sys/eventfd.h>

#include "utils.h"
#include "log.h"

#include "config.h"

#include "kernel/kernel.h"
#include "kernel/vfio.h"
#include "kernel/pci.h"

static const char *vfio_pci_region_names[] = {
	"PCI_BAR0",		// VFIO_PCI_BAR0_REGION_INDEX,
	"PCI_BAR1",		// VFIO_PCI_BAR1_REGION_INDEX,
	"PCI_BAR2",		// VFIO_PCI_BAR2_REGION_INDEX,
	"PCI_BAR3",		// VFIO_PCI_BAR3_REGION_INDEX,
	"PCI_BAR4",		// VFIO_PCI_BAR4_REGION_INDEX,
	"PCI_BAR5",		// VFIO_PCI_BAR5_REGION_INDEX,
	"PCI_ROM",		// VFIO_PCI_ROM_REGION_INDEX,
	"PCI_CONFIG",		// VFIO_PCI_CONFIG_REGION_INDEX,
	"PCI_VGA"		//  VFIO_PCI_INTX_IRQ_INDEX,
};

static const char *vfio_pci_irq_names[] = {
	"PCI_INTX",		// VFIO_PCI_INTX_IRQ_INDEX,
	"PCI_MSI", 		// VFIO_PCI_MSI_IRQ_INDEX,
	"PCI_MSIX",		// VFIO_PCI_MSIX_IRQ_INDEX,
	"PCI_ERR", 		// VFIO_PCI_ERR_IRQ_INDEX,
	"PCI_REQ"		// VFIO_PCI_REQ_IRQ_INDEX,
};

/* Helpers */
int vfio_get_iommu_name(int index, char *buf, size_t len)
{
	FILE *f;
	char fn[256];

	snprintf(fn, sizeof(fn), "/sys/kernel/iommu_groups/%d/name", index);

	f = fopen(fn, "r");
	if (!f)
		return -1;

	int ret = fgets(buf, len, f) == buf ? 0 : -1;
	
	/* Remove trailing newline */
	char *c = strrchr(buf, '\n');
	if (c)
		*c = 0;

	fclose(f);

	return ret;
}

/* Destructors */
int vfio_destroy(struct vfio_container *v)
{
	int ret;

	/* Release memory and close fds */
	list_destroy(&v->groups, (dtor_cb_t) vfio_group_destroy, true);

	/* Close container */
	ret = close(v->fd);
	if (ret < 0)
		return -1;

	debug(5, "VFIO: closed container: fd=%d", v->fd);

	return 0;
}

int vfio_group_destroy(struct vfio_group *g)
{
	int ret;

	list_destroy(&g->devices, (dtor_cb_t) vfio_device_destroy, false);
	
	ret = ioctl(g->fd, VFIO_GROUP_UNSET_CONTAINER);
	if (ret)
		return ret;

	debug(5, "VFIO: released group from container: group=%u", g->index);

	ret = close(g->fd);
	if (ret)
		return ret;

	debug(5, "VFIO: closed group: group=%u, fd=%d", g->index, g->fd);
	
	return 0;
}

int vfio_device_destroy(struct vfio_device *d)
{
	int ret;

	for (int i = 0; i < d->info.num_regions; i++)
		vfio_unmap_region(d, i);

	ret = close(d->fd);
	if (ret)
		return ret;

	debug(5, "VFIO: closed device: name=%s, fd=%d", d->name, d->fd);
	
	free(d->mappings);
	free(d->name);
	
	return 0;
}

/* Constructors */
int vfio_init(struct vfio_container *v)
{
	int ret;

	/* Initialize datastructures */
	memset(v, 0, sizeof(*v));
	
	list_init(&v->groups);

	/* Load VFIO kernel module */
	if (kernel_module_load("vfio"))
		error("Failed to load kernel module: %s", "vfio");

	/* Open VFIO API */
	v->fd = open(VFIO_DEV("vfio"), O_RDWR);
	if (v->fd < 0)
		error("Failed to open VFIO container");

	/* Check VFIO API version */
	v->version = ioctl(v->fd, VFIO_GET_API_VERSION);
	if (v->version < 0 || v->version != VFIO_API_VERSION)
		error("Failed to get VFIO version");

	/* Check available VFIO extensions (IOMMU types) */
	v->extensions = 0;
	for (int i = 1; i < VFIO_DMA_CC_IOMMU; i++) {
		ret = ioctl(v->fd, VFIO_CHECK_EXTENSION, i);
		if (ret < 0)
			error("Failed to get VFIO extensions");
		else if (ret > 0)
			v->extensions |= (1 << i);
	}

	return 0;
}

int vfio_group_attach(struct vfio_group *g, struct vfio_container *c, int index)
{
	int ret;
	char buf[128];

	g->index = index;
	g->container = c;
	
	list_init(&g->devices);
	
	/* Open group fd */
	snprintf(buf, sizeof(buf), VFIO_DEV("%u"), g->index);
	g->fd = open(buf, O_RDWR);
	if (g->fd < 0)
		serror("Failed to open VFIO group: %u", g->index);

	/* Claim group ownership */
	ret = ioctl(g->fd, VFIO_GROUP_SET_CONTAINER, &c->fd);
	if (ret < 0)
		serror("Failed to attach VFIO group to container");
	
	/* Set IOMMU type */
	ret = ioctl(c->fd, VFIO_SET_IOMMU, VFIO_TYPE1_IOMMU);
	if (ret < 0)
		serror("Failed to set IOMMU type of container");

	/* Check group viability and features */
	g->status.argsz = sizeof(g->status);

	ret = ioctl(g->fd, VFIO_GROUP_GET_STATUS, &g->status);
	if (ret < 0)
		serror("Failed to get VFIO group status");
	
	if (!(g->status.flags & VFIO_GROUP_FLAGS_VIABLE))
		error("VFIO group is not available: bind all devices to the VFIO driver!");
	
	list_push(&c->groups, g);

	return 0;
}

int vfio_pci_attach(struct vfio_device *d, struct vfio_container *c, struct pci_device *pdev)
{
	char name[32];
	int ret;
	
	/* Load PCI bus driver for VFIO */
	if (kernel_module_load("vfio_pci"))
		error("Failed to load kernel driver: %s", "vfio_pci");
	
	/* Bind PCI card to vfio-pci driver*/
	ret = pci_attach_driver(pdev, "vfio-pci");
	if (ret)
		error("Failed to attach device to driver");

	/* Get IOMMU group of device */
	int index = pci_get_iommu_group(pdev);
	if (index < 0)
		error("Failed to get IOMMU group of device");

	/* VFIO device name consists of PCI BDF */
	snprintf(name, sizeof(name), "%04x:%02x:%02x.%x", pdev->slot.domain, pdev->slot.bus, pdev->slot.device, pdev->slot.function);

	ret = vfio_device_attach(d, c, name, index);
	if (ret < 0)
		return ret;
	
	/* Check if this is really a vfio-pci device */
	if (!(d->info.flags & VFIO_DEVICE_FLAGS_PCI)) {
		vfio_device_destroy(d);
		return -1;
	}

	d->pci_device = pdev;

	return 0;
}

int vfio_device_attach(struct vfio_device *d, struct vfio_container *c, const char *name, int index)
{
	int ret;
	struct vfio_group *g = NULL;
	
	/* Check if group already exists */
	for (size_t i = 0; i < list_length(&c->groups); i++) {
		struct vfio_group *h = list_at(&c->groups, i);

		if (h->index == index)
			g = h;
	}
	
	if (!g) {
		g = alloc(sizeof(struct vfio_group));

		/* Aquire group ownership */
		ret = vfio_group_attach(g, c, index);
		if (ret)
			error("Failed to attach to IOMMU group: %u", index);
		
		info("Attached new group %u to VFIO container", g->index);
	}

	d->group = g;
	d->name = strdup(name);

	/* Open device fd */
	d->fd = ioctl(g->fd, VFIO_GROUP_GET_DEVICE_FD, d->name);
	if (d->fd < 0)
		serror("Failed to open VFIO device: %s", d->name);

	/* Get device info */
	d->info.argsz = sizeof(d->info);

	ret = ioctl(d->fd, VFIO_DEVICE_GET_INFO, &d->info);
	if (ret < 0)
		serror("Failed to get VFIO device info for: %s", d->name);

	d->irqs     = alloc(d->info.num_irqs    * sizeof(struct vfio_irq_info));
	d->regions  = alloc(d->info.num_regions * sizeof(struct vfio_region_info));
	d->mappings = alloc(d->info.num_regions * sizeof(void *));

	/* Get device regions */
	for (int i = 0; i < d->info.num_regions && i < 8; i++) {
		struct vfio_region_info *region = &d->regions[i];

		region->argsz = sizeof(*region);
		region->index = i;

		ret = ioctl(d->fd, VFIO_DEVICE_GET_REGION_INFO, region);
		if (ret < 0)
			serror("Failed to get regions of VFIO device: %s", d->name);
	}

	/* Get device irqs */
	for (int i = 0; i < d->info.num_irqs; i++) {
		struct vfio_irq_info *irq = &d->irqs[i];

		irq->argsz = sizeof(*irq);
		irq->index = i;

		ret = ioctl(d->fd, VFIO_DEVICE_GET_IRQ_INFO, irq);
		if (ret < 0)
			serror("Failed to get IRQs of VFIO device: %s", d->name);
	}
	
	list_push(&d->group->devices, d);

	return 0;
}

int vfio_pci_reset(struct vfio_device *d)
{
	int ret;

	/* Check if this is really a vfio-pci device */
	if (!(d->info.flags & VFIO_DEVICE_FLAGS_PCI))
		return -1;

	size_t reset_infolen = sizeof(struct vfio_pci_hot_reset_info) + sizeof(struct vfio_pci_dependent_device) * 64;
	size_t resetlen = sizeof(struct vfio_pci_hot_reset) + sizeof(int32_t) * 1;

	struct vfio_pci_hot_reset_info *reset_info = alloc(reset_infolen);
	struct vfio_pci_hot_reset *reset = alloc(resetlen);

	reset_info->argsz = reset_infolen;
	reset->argsz = resetlen;

	ret = ioctl(d->fd, VFIO_DEVICE_GET_PCI_HOT_RESET_INFO, reset_info);
	if (ret)
		return ret;

	debug(5, "VFIO: dependent devices for hot-reset:");
	for (int i = 0; i < reset_info->count; i++) { INDENT
		struct vfio_pci_dependent_device *dd = &reset_info->devices[i];
		debug(5, "%04x:%02x:%02x.%01x: iommu_group=%u", dd->segment, dd->bus, PCI_SLOT(dd->devfn), PCI_FUNC(dd->devfn), dd->group_id);

		if (dd->group_id != d->group->index)
			return -3;
	}

	reset->count = 1;
	reset->group_fds[0] = d->group->fd;

	ret = ioctl(d->fd, VFIO_DEVICE_PCI_HOT_RESET, reset);

	free(reset_info);

	return ret;
}

int vfio_pci_msi_find(struct vfio_device *d, int nos[32])
{
	int ret, idx, irq;
	char *end, *col, *last, line[1024], name[13];
	FILE *f;

	f = fopen("/proc/interrupts", "r");
	if (!f)
		return -1;

	for (int i = 0; i < 32; i++)
		nos[i] = -1;

	/* For each line in /proc/interruipts */
	while (fgets(line, sizeof(line), f)) {
		col = strtok(line, " ");

		/* IRQ number is in first column */
		irq = strtol(col, &end, 10);
		if (col == end)
			continue;

		/* Find last column of line */
		do {
			last = col;
		} while ((col = strtok(NULL, " ")));
			

		ret = sscanf(last, "vfio-msi[%u](%12[0-9:])", &idx, name);
		if (ret == 2) {
			if (strstr(d->name, name) == d->name)
				nos[idx] = irq;
		}
	}

	fclose(f);

	return 0;
}

int vfio_pci_msi_deinit(struct vfio_device *d, int efds[32])
{
	int ret, irq_setlen, irq_count = d->irqs[VFIO_PCI_MSI_IRQ_INDEX].count; 
	struct vfio_irq_set *irq_set;

	/* Check if this is really a vfio-pci device */
	if (!(d->info.flags & VFIO_DEVICE_FLAGS_PCI))
		return -1;

	irq_setlen = sizeof(struct vfio_irq_set) + sizeof(int) * irq_count;
	irq_set = alloc(irq_setlen);

	irq_set->argsz = irq_setlen;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSI_IRQ_INDEX;
	irq_set->count = irq_count;
	irq_set->start = 0;

	for (int i = 0; i < irq_count; i++) {
		close(efds[i]);
		efds[i] = -1;
	}

	memcpy(irq_set->data, efds, sizeof(int) * irq_count);

	ret = ioctl(d->fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret)
		return -4;

	free(irq_set);

	return irq_count;
}

int vfio_pci_msi_init(struct vfio_device *d, int efds[32])
{
	int ret, irq_setlen, irq_count = d->irqs[VFIO_PCI_MSI_IRQ_INDEX].count; 
	struct vfio_irq_set *irq_set;
	
	/* Check if this is really a vfio-pci device */
	if (!(d->info.flags & VFIO_DEVICE_FLAGS_PCI))
		return -1;

	irq_setlen = sizeof(struct vfio_irq_set) + sizeof(int) * irq_count;
	irq_set = alloc(irq_setlen);

	irq_set->argsz = irq_setlen;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSI_IRQ_INDEX;
	irq_set->start = 0;
	irq_set->count = irq_count;

	/* Now set the new eventfds */
	for (int i = 0; i < irq_count; i++) {
		efds[i] = eventfd(0, 0);
		if (efds[i] < 0)
			return -3;
	}
	memcpy(irq_set->data, efds, sizeof(int) * irq_count);

	ret = ioctl(d->fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret)
		return -4;

	free(irq_set);

	return irq_count;
}

int vfio_pci_enable(struct vfio_device *d)
{
	int ret;
	uint32_t reg;
	off_t offset = ((off_t) VFIO_PCI_CONFIG_REGION_INDEX << 40) + PCI_COMMAND;

	/* Check if this is really a vfio-pci device */
	if (!(d->info.flags & VFIO_DEVICE_FLAGS_PCI))
		return -1;

	ret = pread(d->fd, &reg, sizeof(reg), offset);
	if (ret != sizeof(reg))
		return -1;

	/* Enable memory access and PCI bus mastering which is required for DMA */
	reg |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;

	ret = pwrite(d->fd, &reg, sizeof(reg), offset);
	if (ret != sizeof(reg))
		return -1;

	return 0;
}

int vfio_device_reset(struct vfio_device *d)
{
	if (d->info.flags & VFIO_DEVICE_FLAGS_RESET)
		return ioctl(d->fd, VFIO_DEVICE_RESET);
	else
		return -1; /* not supported by this device */
}

void vfio_dump(struct vfio_container *v)
{
	info("VFIO Version: %u", v->version);
	info("VFIO Extensions: %#x", v->extensions);

	for (size_t i = 0; i < list_length(&v->groups); i++) {
		struct vfio_group *g = list_at(&v->groups, i);

		info("VFIO Group %u, viable=%u, container=%d", g->index,
			(g->status.flags & VFIO_GROUP_FLAGS_VIABLE) > 0,
			(g->status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET) > 0
		);


		for (size_t i = 0; i < list_length(&g->devices); i++) { INDENT
			struct vfio_device *d = list_at(&g->devices, i);

			info("Device %s: regions=%u, irqs=%u, flags=%#x", d->name,
				d->info.num_regions,
				d->info.num_irqs,
				d->info.flags
			);

			for (int i = 0; i < d->info.num_regions && i < 8; i++) { INDENT
				struct vfio_region_info *region = &d->regions[i];

				if (region->size > 0)
					info("Region %u %s: size=%#llx, offset=%#llx, flags=%u",
						region->index, (d->info.flags & VFIO_DEVICE_FLAGS_PCI) ? vfio_pci_region_names[i] : "",
						region->size,
						region->offset,
						region->flags
					);
			}
			
			for (int i = 0; i < d->info.num_irqs; i++) { INDENT
				struct vfio_irq_info *irq = &d->irqs[i];

				if (irq->count > 0)
					info("IRQ %u %s: count=%u, flags=%u",
						irq->index, (d->info.flags & VFIO_DEVICE_FLAGS_PCI ) ? vfio_pci_irq_names[i] : "",
						irq->count,
						irq->flags
				);
			}
		}
	}
}

void * vfio_map_region(struct vfio_device *d, int idx)
{
	struct vfio_region_info *r = &d->regions[idx];

	if (!(r->flags & VFIO_REGION_INFO_FLAG_MMAP))
		return MAP_FAILED;

	d->mappings[idx] = mmap(NULL, r->size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_32BIT, d->fd, r->offset);
	
	return d->mappings[idx];
}

int vfio_unmap_region(struct vfio_device *d, int idx)
{
	int ret;
	struct vfio_region_info *r = &d->regions[idx];
	
	if (!d->mappings[idx])
		return -1; /* was not mapped */
	
	debug(3, "VFIO: unmap region %u from device", idx);
	
	ret = munmap(d->mappings[idx], r->size);
	if (ret)
		return -2;
	
	d->mappings[idx] = NULL;

	return 0;
}

int vfio_map_dma(struct vfio_container *c, uint64_t virt, uint64_t phys, size_t len)
{
	int ret;
	
	if (len & 0xFFF) {
		len += 0x1000;
		len &= ~0xFFF;
	}
	
	/* Super stupid allocator */
	if (phys == -1) {
		phys = c->iova_next;
		c->iova_next += len;
	}

	struct vfio_iommu_type1_dma_map dma_map = {
		.argsz = sizeof(struct vfio_iommu_type1_dma_map),
		.vaddr = virt,
		.iova  = phys,
		.size  = len,
		.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE
	};

	ret = ioctl(c->fd, VFIO_IOMMU_MAP_DMA, &dma_map);
	if (ret)
		serror("Failed to create DMA mapping");

	info("DMA map size=%#llx, iova=%#llx, vaddr=%#llx", dma_map.size, dma_map.iova, dma_map.vaddr);

	return 0;
}

int vfio_unmap_dma(struct vfio_container *c, uint64_t virt, uint64_t phys, size_t len)
{
	int ret;
	
	struct vfio_iommu_type1_dma_unmap dma_unmap = {
		.argsz = sizeof(struct vfio_iommu_type1_dma_unmap),
		.flags = 0,
		.iova  = phys,
		.size  = len,
	};
	
	ret = ioctl(c->fd, VFIO_IOMMU_UNMAP_DMA, &dma_unmap);
	if (ret)
		serror("Failed to unmap DMA mapping");
	
	return 0;
}
