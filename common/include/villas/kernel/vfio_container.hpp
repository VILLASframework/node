/** Virtual Function IO wrapper around kernel API
 *
 * @file
 * @author Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2022, Niklas Eiling
 * @copyright 2014-2021, Steffen Vogel
 * @copyright 2018, Daniel Krebs
 *********************************************************************************/

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
	std::shared_ptr<Device> attachDevice(const pci::Device &pdev);

	/**
	 * @brief Map VM to an IOVA, which is accessible by devices in the container
	 * @param virt		virtual address of memory
	 * @param phys		IOVA where to map @p virt, -1 to use VFIO internal allocator
	 * @param length	size of memory region in bytes
	 * @return		IOVA address, UINTPTR_MAX on failure
	 */
	uintptr_t memoryMap(uintptr_t virt, uintptr_t phys, size_t length);

	/** munmap() a region which has been mapped by vfio_map_region() */
	bool memoryUnmap(uintptr_t phys, size_t length);

	bool isIommuEnabled() const
	{
		return this->hasIommu;
	}

private:
	std::shared_ptr<Group> getOrAttachGroup(int index);

	int fd;
	int version;
	int extensions;
	uint64_t iova_next;			/**< Next free IOVA address */
	bool hasIommu;

	// All groups bound to this container
	std::list<std::shared_ptr<Group>> groups;

	Logger log;
};

} // namespace vfio
} // namespace kernel
} // namespace villas
