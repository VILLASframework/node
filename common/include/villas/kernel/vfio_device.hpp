/* Virtual Function IO wrapper around kernel API
 *
 * @file
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @author Steffen Vogel <post@steffenvogel.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2022, Niklas Eiling
 * @copyright 2014-2021, Steffen Vogel
 * @copyright 2018, Daniel Krebs
 */

#pragma once

#include <list>
#include <vector>
#include <memory>
#include <string>

#include <linux/vfio.h>
#include <sys/mman.h>

#include <villas/kernel/pci.hpp>
#include <villas/log.hpp>

namespace villas {
namespace kernel {
namespace vfio {

class Device {
public:
	Device(const std::string &name, int groupFileDescriptor, const kernel::pci::Device *pci_device = nullptr);
	~Device();

	// No copying allowed because we manage the vfio state in constructor and destructors
	Device(Device const&) = delete;
	void operator=(Device const&) = delete;

	bool reset();

	// Map a device memory region to the application address space (e.g. PCI BARs)
	void* regionMap(size_t index);

	// munmap() a region which has been mapped by vfio_map_region()
	bool regionUnmap(size_t index);

	// Get the size of a device memory region
	size_t regionGetSize(size_t index);

	// Enable memory accesses and bus mastering for PCI device
	bool pciEnable();

	int pciMsiInit(int efds[32]);
	int pciMsiDeinit(int efds[32]);
	bool pciMsiFind(int nos[32]);

	bool isVfioPciDevice() const;
	bool pciHotReset();

	int getFileDescriptor() const
	{
		return fd;
	}

	void dump();

	bool isAttachedToGroup() const
	{
		return attachedToGroup;
	}

	void setAttachedToGroup()
	{
		this->attachedToGroup = true;
	}

private:
	// Name of the device as listed under
	// /sys/kernel/iommu_groups/[vfio_group::index]/devices/
	std::string name;

	// VFIO device file descriptor
	int fd;

	bool attachedToGroup;
	int groupFd;

	struct vfio_device_info info;

	std::vector<struct vfio_irq_info> irqs;
	std::vector<struct vfio_region_info> regions;
	std::vector<void*> mappings;

	// libpci handle of the device
	const kernel::pci::Device *pci_device;

	Logger log;
};

} // namespace vfio
} // namespace kernel
} // namespace villas
