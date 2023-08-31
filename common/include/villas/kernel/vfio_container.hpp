/* Virtual Function IO wrapper around kernel API
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2022-2023 Niklas Eiling
 * SPDX-FileCopyrightText: 2014-2023 Steffen Vogel
 * SPDX-FileCopyrightText: 2018 Daniel Krebs
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <list>
#include <vector>
#include <memory>

#include <linux/vfio.h>
#include <sys/mman.h>

#include <villas/kernel/vfio_group.hpp>

namespace villas {
namespace kernel {
namespace vfio {

// Backwards compatability with older kernels
#ifdef VFIO_UPDATE_VADDR
static constexpr
size_t EXTENSION_SIZE = VFIO_UPDATE_VADDR+1;
#elif defined(VFIO_UNMAP_ALL)
static constexpr
size_t EXTENSION_SIZE = VFIO_UNMAP_ALL+1;
#else
static constexpr
size_t EXTENSION_SIZE = VFIO_NOIOMMU_IOMMU+1;
#endif

class Container {
public:
	Container();

	// No copying allowed because we manage the vfio state in constructor and destructors
	Container(Container const&) = delete;
	void operator=(Container const&) = delete;

	~Container();

	void dump();

	void attachGroup(std::shared_ptr<Group> group);

	std::shared_ptr<Device> attachDevice(const std::string& name, int groupIndex);
	std::shared_ptr<Device> attachDevice(pci::Device &pdev);

	// Map VM to an IOVA, which is accessible by devices in the container
	//
	// @param virt		virtual address of memory
	// @param phys		IOVA where to map @p virt, -1 to use VFIO internal allocator
	// @param length	size of memory region in bytes
	// @return		IOVA address, UINTPTR_MAX on failure
	uintptr_t memoryMap(uintptr_t virt, uintptr_t phys, size_t length);

	// munmap() a region which has been mapped by vfio_map_region()
	bool memoryUnmap(uintptr_t phys, size_t length);

	bool isIommuEnabled() const
	{
		return this->hasIommu;
	}

private:
	std::shared_ptr<Group> getOrAttachGroup(int index);

	int fd;
	int version;

	std::array<bool, EXTENSION_SIZE> extensions;
	uint64_t iova_next;			// Next free IOVA address
	bool hasIommu;

	// All groups bound to this container
	std::list<std::shared_ptr<Group>> groups;

	Logger log;
};

} // namespace vfio
} // namespace kernel
} // namespace villas
