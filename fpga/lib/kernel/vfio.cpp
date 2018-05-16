/** Virtual Function IO wrapper around kernel API
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017, Steffen Vogel
 * @copyright 2018, Daniel Krebs
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
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

#define _DEFAULT_SOURCE

#include <algorithm>
#include <string>
#include <sstream>
#include <limits>

#include <cstdlib>
#include <cstring>

#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <linux/pci_regs.h>

#include "kernel/pci.h"
#include "kernel/kernel.h"

#include "kernel/vfio.hpp"
#include "log.hpp"

static auto logger = loggerGetOrCreate("Vfio");

static const char *vfio_pci_region_names[] = {
    "PCI_BAR0",		// VFIO_PCI_BAR0_REGION_INDEX,
    "PCI_BAR1",		// VFIO_PCI_BAR1_REGION_INDEX,
    "PCI_BAR2",		// VFIO_PCI_BAR2_REGION_INDEX,
    "PCI_BAR3",		// VFIO_PCI_BAR3_REGION_INDEX,
    "PCI_BAR4",		// VFIO_PCI_BAR4_REGION_INDEX,
    "PCI_BAR5",		// VFIO_PCI_BAR5_REGION_INDEX,
    "PCI_ROM",		// VFIO_PCI_ROM_REGION_INDEX,
    "PCI_CONFIG",	// VFIO_PCI_CONFIG_REGION_INDEX,
    "PCI_VGA"		//  VFIO_PCI_INTX_IRQ_INDEX,
};

static const char *vfio_pci_irq_names[] = {
    "PCI_INTX",		// VFIO_PCI_INTX_IRQ_INDEX,
    "PCI_MSI", 		// VFIO_PCI_MSI_IRQ_INDEX,
    "PCI_MSIX",		// VFIO_PCI_MSIX_IRQ_INDEX,
    "PCI_ERR", 		// VFIO_PCI_ERR_IRQ_INDEX,
    "PCI_REQ"		// VFIO_PCI_REQ_IRQ_INDEX,
};

namespace villas {


VfioContainer::VfioContainer()
    : iova_next(0)
{

	static constexpr const char* requiredKernelModules[] = {
	    "vfio", "vfio_pci", "vfio_iommu_type1"
	};

	for(const char* module : requiredKernelModules) {
		if(kernel_module_loaded(module) != 0) {
			logger->error("Kernel module '{}' required but not loaded. "
			              "Please load manually!", module);
			throw std::exception();
		}
	}

	/* Open VFIO API */
	fd = open(VFIO_DEV, O_RDWR);
	if (fd < 0) {
		logger->error("Failed to open VFIO container");
		throw std::exception();
	}

	/* Check VFIO API version */
	version = ioctl(fd, VFIO_GET_API_VERSION);
	if (version < 0 || version != VFIO_API_VERSION) {
		logger->error("Failed to get VFIO version");
		throw std::exception();
	}

	/* Check available VFIO extensions (IOMMU types) */
	extensions = 0;
	for (unsigned int i = VFIO_TYPE1_IOMMU; i <= VFIO_NOIOMMU_IOMMU; i++) {
		int ret = ioctl(fd, VFIO_CHECK_EXTENSION, i);
		if (ret < 0) {
			logger->error("Failed to get VFIO extensions");
			throw std::exception();
		}
		else if (ret > 0) {
			extensions |= (1 << i);
		}
	}

	hasIommu = false;

	if(not (extensions & (1 << VFIO_NOIOMMU_IOMMU))) {
		if(not (extensions & (1 << VFIO_TYPE1_IOMMU))) {
			logger->error("No supported IOMMU extension found");
			throw std::exception();
		} else {
			hasIommu = true;
		}
	}

	logger->debug("Version:    {:#x}", version);
	logger->debug("Extensions: {:#x}", extensions);
	logger->debug("IOMMU:      {}", hasIommu ? "yes" : "no");
}


VfioContainer::~VfioContainer()
{
	logger->debug("Clean up container with fd {}", fd);

	/* Release memory and close fds */
	groups.clear();

	/* Close container */
	int ret = close(fd);
	if (ret < 0) {
		logger->error("Cannot close vfio container fd {}", fd);
	}
}


std::shared_ptr<VfioContainer>
VfioContainer::create()
{
	std::shared_ptr<VfioContainer> container { new VfioContainer };
	return container;
}


void
VfioContainer::dump()
{
	logger->info("File descriptor: {}", fd);
	logger->info("Version: {}", version);
	logger->info("Extensions: 0x{:x}", extensions);

	for(auto& group : groups) {
		logger->info("VFIO Group {}, viable={}, container={}",
		             group->index,
		             (group->status.flags & VFIO_GROUP_FLAGS_VIABLE) > 0,
		             (group->status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET) > 0
		);

		for(auto& device : group->devices) {
			logger->info("Device {}: regions={}, irqs={}, flags={}",
			             device->name,
			             device->info.num_regions,
			             device->info.num_irqs,
			             device->info.flags
			);

			for (size_t i = 0; i < device->info.num_regions && i < 8; i++) {
				struct vfio_region_info *region = &device->regions[i];

				if (region->size > 0)
					logger->info("Region {} {}: size={}, offset={}, flags={}",
					             region->index,
					             (device->info.flags & VFIO_DEVICE_FLAGS_PCI) ?
					                 vfio_pci_region_names[i] : "",
					             region->size,
					             region->offset,
					             region->flags
					);
			}

			for (size_t i = 0; i < device->info.num_irqs; i++) {
				struct vfio_irq_info *irq = &device->irqs[i];

				if (irq->count > 0)
					logger->info("IRQ {} {}: count={}, flags={}",
					             irq->index,
					             (device->info.flags & VFIO_DEVICE_FLAGS_PCI ) ?
					                 vfio_pci_irq_names[i] : "",
					             irq->count,
					             irq->flags
				);
			}
		}
	}
}


VfioDevice&
VfioContainer::attachDevice(const char* name, int index)
{
	VfioGroup& group = getOrAttachGroup(index);
	auto device = std::make_unique<VfioDevice>(name, group);

	/* Open device fd */
	device->fd = ioctl(group.fd, VFIO_GROUP_GET_DEVICE_FD, name);
	if (device->fd < 0) {
		logger->error("Failed to open VFIO device: {}", device->name);
		throw std::exception();
	}

	/* Get device info */
	device->info.argsz = sizeof(device->info);

	int ret = ioctl(device->fd, VFIO_DEVICE_GET_INFO, &device->info);
	if (ret < 0) {
		logger->error("Failed to get VFIO device info for: {}", device->name);
		throw std::exception();
	}

	logger->debug("Device has {} regions", device->info.num_regions);
	logger->debug("Device has {} IRQs", device->info.num_irqs);

	// reserve slots already so that we can use the []-operator for access
	device->irqs.resize(device->info.num_irqs);
	device->regions.resize(device->info.num_regions);
	device->mappings.resize(device->info.num_regions);

	/* Get device regions */
	for (size_t i = 0; i < device->info.num_regions && i < 8; i++) {
		struct vfio_region_info region;
		memset(&region, 0, sizeof (region));

		region.argsz = sizeof(region);
		region.index = i;

		ret = ioctl(device->fd, VFIO_DEVICE_GET_REGION_INFO, &region);
		if (ret < 0) {
			logger->error("Failed to get region of VFIO device: {}", device->name);
			throw std::exception();
		}

		device->regions[i] = region;
	}


	/* Get device irqs */
	for (size_t i = 0; i < device->info.num_irqs; i++) {
		struct vfio_irq_info irq;
		memset(&irq, 0, sizeof (irq));

		irq.argsz = sizeof(irq);
		irq.index = i;

		ret = ioctl(device->fd, VFIO_DEVICE_GET_IRQ_INFO, &irq);
		if (ret < 0) {
			logger->error("Failed to get IRQs of VFIO device: {}", device->name);
			throw std::exception();
		}

		device->irqs[i] = irq;
	}

	group.devices.push_back(std::move(device));

	return *group.devices.back().get();
}


VfioDevice&
VfioContainer::attachDevice(const pci_device* pdev)
{
	int ret;
	char name[32];
	static constexpr const char* kernelDriver = "vfio-pci";

	/* Load PCI bus driver for VFIO */
	if (kernel_module_load("vfio_pci")) {
		logger->error("Failed to load kernel driver: vfio_pci");
		throw std::exception();
	}

	/* Bind PCI card to vfio-pci driver if not already bound */
	ret = pci_get_driver(pdev, name, sizeof(name));
	if (ret || strcmp(name, kernelDriver)) {
		logger->debug("Bind PCI card to kernel driver '{}'", kernelDriver);
		ret = pci_attach_driver(pdev, kernelDriver);
		if (ret) {
			logger->error("Failed to attach device to driver");
			throw std::exception();
		}
	}

	/* Get IOMMU group of device */
	int index = isIommuEnabled() ? pci_get_iommu_group(pdev) : 0;
	if (index < 0) {
		logger->error("Failed to get IOMMU group of device");
		throw std::exception();
	}

	/* VFIO device name consists of PCI BDF */
	snprintf(name, sizeof(name), "%04x:%02x:%02x.%x", pdev->slot.domain,
	         pdev->slot.bus, pdev->slot.device, pdev->slot.function);

	logger->info("Attach to device {} with index {}", std::string(name), index);
	auto& device = attachDevice(name, index);

	device.pci_device = pdev;

	/* Check if this is really a vfio-pci device */
	if(not device.isVfioPciDevice()) {
		logger->error("Device is not a vfio-pci device");
		throw std::exception();
	}

	return device;
}


uintptr_t
VfioContainer::memoryMap(uintptr_t virt, uintptr_t phys, size_t length)
{
	int ret;

	if(not hasIommu) {
		logger->error("DMA mapping not supported without IOMMU");
		return UINTPTR_MAX;
	}

	if (length & 0xFFF) {
		length += 0x1000;
		length &= ~0xFFF;
	}

	/* Super stupid allocator */
	size_t iovaIncrement = 0;
	if (phys == UINTPTR_MAX) {
		phys = this->iova_next;
		iovaIncrement = length;
	}

	struct vfio_iommu_type1_dma_map dmaMap;
	memset(&dmaMap, 0, sizeof(dmaMap));

	dmaMap.argsz = sizeof(dmaMap);
	dmaMap.vaddr = virt;
	dmaMap.iova  = phys;
	dmaMap.size  = length;
	dmaMap.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

	ret = ioctl(this->fd, VFIO_IOMMU_MAP_DMA, &dmaMap);
	if (ret) {
		logger->error("Failed to create DMA mapping: {}", ret);
		return UINTPTR_MAX;
	}

	logger->debug("DMA map size={:#x}, iova={:#x}, vaddr={:#x}",
	              dmaMap.size, dmaMap.iova, dmaMap.vaddr);

	// mapping successful, advance IOVA allocator
	this->iova_next += iovaIncrement;

	// we intentionally don't return the actual mapped length, the users are
	// only guaranteed to have their demanded memory mapped correctly
	return dmaMap.iova;
}


bool
VfioContainer::memoryUnmap(uintptr_t phys, size_t length)
{
	int ret;

	if(not hasIommu) {
		return true;
	}

	struct vfio_iommu_type1_dma_unmap dmaUnmap;
	dmaUnmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
	dmaUnmap.flags = 0;
	dmaUnmap.iova  = phys;
	dmaUnmap.size  = length;

	ret = ioctl(this->fd, VFIO_IOMMU_UNMAP_DMA, &dmaUnmap);
	if (ret) {
		logger->error("Failed to unmap DMA mapping");
		return false;
	}

	return true;
}


VfioGroup&
VfioContainer::getOrAttachGroup(int index)
{
	// search if group with index already exists
	for(auto& group : groups) {
		if(group->index == index) {
			return *group;
		}
	}

	// group not yet part of this container, so acquire ownership
	auto group = VfioGroup::attach(*this, index);
	if(not group) {
		logger->error("Failed to attach to IOMMU group: {}", index);
		throw std::exception();
	} else {
		logger->debug("Attached new group {} to VFIO container", index);
	}

	// push to our list
	groups.push_back(std::move(group));

	return *groups.back();
}


VfioDevice::~VfioDevice()
{
	logger->debug("Clean up device {} with fd {}", this->name, this->fd);

	for(auto& region : regions) {
		regionUnmap(region.index);
	}

	int ret = close(fd);
	if (ret != 0) {
		logger->error("Closing device fd {} failed", fd);
	}
}


bool
VfioDevice::reset()
{
	if (this->info.flags & VFIO_DEVICE_FLAGS_RESET)
		return ioctl(this->fd, VFIO_DEVICE_RESET) == 0;
	else
		return false; /* not supported by this device */
}


void*
VfioDevice::regionMap(size_t index)
{
	struct vfio_region_info *r = &regions[index];

	if (!(r->flags & VFIO_REGION_INFO_FLAG_MMAP))
		return MAP_FAILED;

	mappings[index] = mmap(nullptr, r->size,
	                       PROT_READ | PROT_WRITE,
	                       MAP_SHARED | MAP_32BIT,
	                       fd, r->offset);

	return mappings[index];
}


bool
VfioDevice::regionUnmap(size_t index)
{
	int ret;
	struct vfio_region_info *r = &regions[index];

	if (!mappings[index])
		return false; /* was not mapped */

	logger->debug("Unmap region {} from device {}", index, name);

	ret = munmap(mappings[index], r->size);
	if (ret)
		return false;

	mappings[index] = nullptr;

	return true;
}


size_t
VfioDevice::regionGetSize(size_t index)
{
	if(index >= regions.size()) {
		logger->error("Index out of range: {} >= {}", index, regions.size());
		throw std::out_of_range("Index out of range");
	}

	return regions[index].size;
}


bool
VfioDevice::pciEnable()
{
	int ret;
	uint32_t reg;
	const off_t offset = PCI_COMMAND +
	                     (static_cast<off_t>(VFIO_PCI_CONFIG_REGION_INDEX) << 40);

	/* Check if this is really a vfio-pci device */
	if (!(this->info.flags & VFIO_DEVICE_FLAGS_PCI))
		return false;

	ret = pread(this->fd, &reg, sizeof(reg), offset);
	if (ret != sizeof(reg))
		return false;

	/* Enable memory access and PCI bus mastering which is required for DMA */
	reg |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;

	ret = pwrite(this->fd, &reg, sizeof(reg), offset);
	if (ret != sizeof(reg))
		return false;

	return true;
}


bool
VfioDevice::pciHotReset()
{
	/* Check if this is really a vfio-pci device */
	if (not isVfioPciDevice())
		return false;

	const size_t reset_infolen = sizeof(struct vfio_pci_hot_reset_info) +
	                             sizeof(struct vfio_pci_dependent_device) * 64;

	auto reset_info = reinterpret_cast<struct vfio_pci_hot_reset_info*>
	                    (calloc(1, reset_infolen));

	reset_info->argsz = reset_infolen;


	if (ioctl(this->fd, VFIO_DEVICE_GET_PCI_HOT_RESET_INFO, reset_info) != 0) {
		free(reset_info);
		return false;
	}

	logger->debug("Dependent devices for hot-reset:");
	for (size_t i = 0; i < reset_info->count; i++) {
		struct vfio_pci_dependent_device *dd = &reset_info->devices[i];
		logger->debug("  {:04x}:{:02x}:{:02x}.{:01x}: iommu_group={}",
		              dd->segment, dd->bus,
		              PCI_SLOT(dd->devfn), PCI_FUNC(dd->devfn), dd->group_id);

		if (static_cast<int>(dd->group_id) != this->group.index) {
			free(reset_info);
			return false;
		}
	}

	free(reset_info);

	const size_t resetlen = sizeof(struct vfio_pci_hot_reset) +
	                        sizeof(int32_t) * 1;
	auto reset = reinterpret_cast<struct vfio_pci_hot_reset*>
	                (calloc(1, resetlen));

	reset->argsz = resetlen;
	reset->count = 1;
	reset->group_fds[0] = this->group.fd;

	int ret = ioctl(this->fd, VFIO_DEVICE_PCI_HOT_RESET, reset);
	const bool success = (ret == 0);

	free(reset);

	if(not success and not group.container->isIommuEnabled()) {
		logger->info("PCI hot reset failed, but this is expected without IOMMU");
		return true;
	}

	return success;
}


int
VfioDevice::pciMsiInit(int efds[])
{
	/* Check if this is really a vfio-pci device */
	if(not isVfioPciDevice())
		return -1;

	const size_t irqCount = irqs[VFIO_PCI_MSI_IRQ_INDEX].count;
	const size_t irqSetSize = sizeof(struct vfio_irq_set) +
	                          sizeof(int) * irqCount;

	auto irqSet = reinterpret_cast<struct vfio_irq_set*>(calloc(1, irqSetSize));
	if(irqSet == nullptr)
		return -1;

	irqSet->argsz = irqSetSize;
	irqSet->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irqSet->index = VFIO_PCI_MSI_IRQ_INDEX;
	irqSet->start = 0;
	irqSet->count = irqCount;

	/* Now set the new eventfds */
	for (size_t i = 0; i < irqCount; i++) {
		efds[i] = eventfd(0, 0);
		if (efds[i] < 0) {
			free(irqSet);
			return -1;
		}
	}

	memcpy(irqSet->data, efds, sizeof(int) * irqCount);

	if(ioctl(fd, VFIO_DEVICE_SET_IRQS, irqSet) != 0) {
		free(irqSet);
		return -1;
	}

	free(irqSet);

	return irqCount;
}


int
VfioDevice::pciMsiDeinit(int efds[])
{
	/* Check if this is really a vfio-pci device */
	if(not isVfioPciDevice())
		return -1;

	const size_t irqCount = irqs[VFIO_PCI_MSI_IRQ_INDEX].count;
	const size_t irqSetSize = sizeof(struct vfio_irq_set) +
	                            sizeof(int) * irqCount;

	auto irqSet = reinterpret_cast<struct vfio_irq_set*>(calloc(1, irqSetSize));
	if(irqSet == nullptr)
		return -1;

	irqSet->argsz = irqSetSize;
	irqSet->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
	irqSet->index = VFIO_PCI_MSI_IRQ_INDEX;
	irqSet->count = irqCount;
	irqSet->start = 0;

	for (size_t i = 0; i < irqCount; i++) {
		close(efds[i]);
		efds[i] = -1;
	}

	memcpy(irqSet->data, efds, sizeof(int) * irqCount);

	if (ioctl(fd, VFIO_DEVICE_SET_IRQS, irqSet) != 0) {
		free(irqSet);
		return -1;
	}

	free(irqSet);

	return irqCount;
}


bool
VfioDevice::pciMsiFind(int nos[])
{
	int ret, idx, irq;
	char *end, *col, *last, line[1024], name[13];
	FILE *f;

	f = fopen("/proc/interrupts", "r");
	if (!f)
		return false;

	for (int i = 0; i < 32; i++)
		nos[i] = -1;

	/* For each line in /proc/interrupts */
	while (fgets(line, sizeof(line), f)) {
		col = strtok(line, " ");

		/* IRQ number is in first column */
		irq = strtol(col, &end, 10);
		if (col == end)
			continue;

		/* Find last column of line */
		do {
			last = col;
		} while ((col = strtok(nullptr, " ")));


		ret = sscanf(last, "vfio-msi[%u](%12[0-9:])", &idx, name);
		if (ret == 2) {
			if (strstr(this->name.c_str(), name) == this->name.c_str())
				nos[idx] = irq;
		}
	}

	fclose(f);

	return true;
}


bool
VfioDevice::isVfioPciDevice() const
{
	return info.flags & VFIO_DEVICE_FLAGS_PCI;
}


VfioGroup::~VfioGroup()
{
	logger->debug("Clean up group {} with fd {}", this->index, this->fd);

	/* Release memory and close fds */
	devices.clear();

	if(fd < 0) {
		logger->debug("Destructing group that has not been attached");
	} else {
		int ret = ioctl(fd, VFIO_GROUP_UNSET_CONTAINER);
		if (ret != 0) {
			logger->error("Cannot unset container for group fd {}", fd);
		}

		ret = close(fd);
		if (ret != 0) {
			logger->error("Cannot close group fd {}", fd);
		}
	}
}


std::unique_ptr<VfioGroup>
VfioGroup::attach(VfioContainer& container, int groupIndex)
{
	std::unique_ptr<VfioGroup> group { new VfioGroup(groupIndex) };

	group->container = &container;

	/* Open group fd */
	std::stringstream groupPath;
	groupPath << VFIO_PATH
	          << (container.isIommuEnabled() ? "" : "noiommu-")
	          << groupIndex;

	group->fd = open(groupPath.str().c_str(), O_RDWR);
	if (group->fd < 0) {
		logger->error("Failed to open VFIO group {}", group->index);
		return nullptr;
	}

	logger->debug("VFIO group {} (fd {}) has path {}",
	              groupIndex, group->fd, groupPath.str());

	/* Claim group ownership */
	int ret = ioctl(group->fd, VFIO_GROUP_SET_CONTAINER, &container.getFd());
	if (ret < 0) {
		logger->error("Failed to attach VFIO group {} to container fd {} (error {})",
		              group->index, container.getFd(), ret);
		return nullptr;
	}

	/* Set IOMMU type */
	int iommu_type = container.isIommuEnabled() ?
	                     VFIO_TYPE1_IOMMU :
	                     VFIO_NOIOMMU_IOMMU;

	ret = ioctl(container.getFd(), VFIO_SET_IOMMU, iommu_type);
	if (ret < 0) {
		logger->error("Failed to set IOMMU type of container: {}", ret);
		return nullptr;
	}

	/* Check group viability and features */
	group->status.argsz = sizeof(group->status);

	ret = ioctl(group->fd, VFIO_GROUP_GET_STATUS, &group->status);
	if (ret < 0) {
		logger->error("Failed to get VFIO group status");
		return nullptr;
	}

	if (!(group->status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		logger->error("VFIO group is not available: bind all devices to the VFIO driver!");
		return nullptr;
	}

	return group;
}

} // namespace villas

