/** Virtual Function IO wrapper around kernel API
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2014-2021, Steffen Vogel
 * @copyright 2018, Daniel Krebs
 * @license Apache License 2.0
 *********************************************************************************/

#define _DEFAULT_SOURCE

#if defined(__arm__) || defined(__aarch64__)
  #define _LARGEFILE64_SOURCE 1
  #define _FILE_OFFSET_BITS 64
#endif

#include <algorithm>
#include <string>
#include <sstream>
#include <limits>

#include <cstdlib>
#include <cstring>

#include <cstdint>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/eventfd.h>
#include <linux/pci_regs.h>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/kernel/vfio_container.hpp>
#include <villas/kernel/kernel.hpp>

using namespace villas::kernel::vfio;


Container::Container() :
	fd(-1),
	version(0),
	extensions(0),
	iova_next(0),
	hasIommu(false),
	groups(),
	log(logging.get("kernel:vfio::Container"))
{
	static constexpr const char* requiredKernelModules[] = {
	    "vfio", "vfio_pci", "vfio_iommu_type1"
	};

	for (const char* module : requiredKernelModules) {
		if (kernel::loadModule(module) != 0)
			throw RuntimeError("Kernel module '{}' required but could not be loaded. "
			              "Please load manually!", module);
	}

	// Open VFIO API
	fd = open(VFIO_DEV, O_RDWR);
	if (fd < 0)
		throw RuntimeError("Failed to open VFIO container");

	// Check VFIO API version
	version = ioctl(fd, VFIO_GET_API_VERSION);
	if (version < 0 || version != VFIO_API_VERSION)
		throw RuntimeError("Failed to get VFIO version");

	// Check available VFIO extensions (IOMMU types)
	extensions = 0;
	for (unsigned int i = VFIO_TYPE1_IOMMU; i <= VFIO_NOIOMMU_IOMMU; i++) {
		int ret = ioctl(fd, VFIO_CHECK_EXTENSION, i);
		if (ret < 0)
			throw RuntimeError("Failed to get VFIO extensions");
		else if (ret > 0)
			extensions |= (1 << i);
	}

	hasIommu = false;

	if (not (extensions & (1 << VFIO_NOIOMMU_IOMMU))) {
		if (not (extensions & (1 << VFIO_TYPE1_IOMMU)))
			throw RuntimeError("No supported IOMMU extension found");
		else
			hasIommu = true;
	}

	log->debug("Version:    {:#x}", version);
	log->debug("Extensions: {:#x}", extensions);
	log->debug("IOMMU:      {}", hasIommu ? "yes" : "no");
}

Container::~Container()
{
	// Release memory and close fds
	groups.clear();

	log->debug("Cleaning up container with fd {}", fd);

	// Close container
	int ret = close(fd);
	if (ret < 0)
		log->error("Error closing vfio container fd {}: {}", fd, ret);
}

void Container::attachGroup(std::shared_ptr<Group> group)
{
	if (group->isAttachedToContainer())
		throw RuntimeError("Group is already attached to a container");

	// Claim group ownership
	int ret = ioctl(group->getFileDescriptor(), VFIO_GROUP_SET_CONTAINER, &fd);
	if (ret < 0) {
		log->error("Failed to attach VFIO group {} to container fd {} (error {})",
		              group->getIndex(), fd, ret);
		throw RuntimeError("Failed to attach VFIO group to container");
	}

	// Set IOMMU type
	int iommu_type = isIommuEnabled() ? VFIO_TYPE1_IOMMU : VFIO_NOIOMMU_IOMMU;

	ret = ioctl(fd, VFIO_SET_IOMMU, iommu_type);
	if (ret < 0) {
		log->error("Failed to set IOMMU type of container: {}", ret);
		throw RuntimeError("Failed to set IOMMU type of container");
	}

	group->setAttachedToContainer();

	if (!group->checkStatus())
		throw RuntimeError("bad VFIO group status for group {}.", group->getIndex());
	else
		log->debug("Attached new group {} to VFIO container", group->getIndex());

	// Push to our list
	groups.push_back(std::move(group));
}

std::shared_ptr<Group> Container::getOrAttachGroup(int index)
{
	// Search if group with index already exists
	for (auto &group : groups) {
		if (group->getIndex() == index) {
			return group;
		}
	}

	// Group not yet part of this container, so acquire ownership
	auto group = std::make_shared<Group>(index, isIommuEnabled());
	attachGroup(group);


	return group;
}

void Container::dump()
{
	log->info("File descriptor: {}", fd);
	log->info("Version: {}", version);
	log->info("Extensions: 0x{:x}", extensions);

	for (auto &group : groups) {
		group->dump();
	}
}

std::shared_ptr<Device> Container::attachDevice(const std::string& name, int index)
{
	auto group = getOrAttachGroup(index);
	auto device = group->attachDevice(name);

	return device;
}

std::shared_ptr<Device> Container::attachDevice(const pci::Device &pdev)
{
	int ret;
	char name[32], iommu_state[4];
	static constexpr const char* kernelDriver = "vfio-pci";

	// Load PCI bus driver for VFIO
	if (kernel::loadModule("vfio_pci"))
		throw RuntimeError("Failed to load kernel driver: vfio_pci");

	// Bind PCI card to vfio-pci driver if not already bound
	if (pdev.getDriver() != kernelDriver) {
		log->debug("Bind PCI card to kernel driver '{}'", kernelDriver);
		pdev.attachDriver(kernelDriver);
	}

	// Get IOMMU group of device
	int index = isIommuEnabled() ? pdev.getIOMMUGroup() : 0;
	if (index < 0) {
		ret = kernel::getCmdlineParam("intel_iommu", iommu_state, sizeof(iommu_state));
		if (ret != 0 || strcmp("on", iommu_state) != 0)
			log->warn("Kernel booted without command line parameter "
					"'intel_iommu' set to 'on'. Please check documentation "
					"(https://villas.fein-aachen.org/doc/fpga-setup.html) "
					"for help with troubleshooting.");

		throw RuntimeError("Failed to get IOMMU group of device");
	}

	// VFIO device name consists of PCI BDF
	snprintf(name, sizeof(name), "%04x:%02x:%02x.%x", pdev.slot.domain,
	         pdev.slot.bus, pdev.slot.device, pdev.slot.function);

	log->info("Attach to device {} with index {}", std::string(name), index);
	auto group = getOrAttachGroup(index);
	auto device = group->attachDevice(name, &pdev);


	// Check if this is really a vfio-pci device
	if (!device->isVfioPciDevice())
		throw RuntimeError("Device is not a vfio-pci device");

	return device;
}


uintptr_t Container::memoryMap(uintptr_t virt, uintptr_t phys, size_t length)
{
	int ret;

	if (not hasIommu) {
		log->error("DMA mapping not supported without IOMMU");
		return UINTPTR_MAX;
	}

	if (length & 0xFFF) {
		length += 0x1000;
		length &= ~0xFFF;
	}

	// Super stupid allocator
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
		log->error("Failed to create DMA mapping: {}", ret);
		return UINTPTR_MAX;
	}

	log->debug("DMA map size={:#x}, iova={:#x}, vaddr={:#x}",
	              dmaMap.size, dmaMap.iova, dmaMap.vaddr);

	// Mapping successful, advance IOVA allocator
	this->iova_next += iovaIncrement;

	// We intentionally don't return the actual mapped length, the users are
	// only guaranteed to have their demanded memory mapped correctly
	return dmaMap.iova;
}

bool Container::memoryUnmap(uintptr_t phys, size_t length)
{
	int ret;

	if (not hasIommu)
		return true;

	struct vfio_iommu_type1_dma_unmap dmaUnmap;
	dmaUnmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
	dmaUnmap.flags = 0;
	dmaUnmap.iova  = phys;
	dmaUnmap.size  = length;

	ret = ioctl(this->fd, VFIO_IOMMU_UNMAP_DMA, &dmaUnmap);
	if (ret) {
		log->error("Failed to unmap DMA mapping");
		return false;
	}

	return true;
}

