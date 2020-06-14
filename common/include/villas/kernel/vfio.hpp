/** Virtual Function IO wrapper around kernel API
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2014-2020, Steffen Vogel
 * @copyright 2018, Daniel Krebs
 *********************************************************************************/

/** @addtogroup fpga Kernel @{ */

#pragma once

#include <list>
#include <vector>
#include <memory>

#include <linux/vfio.h>
#include <sys/mman.h>

#define VFIO_PATH	"/dev/vfio/"
#define VFIO_DEV	VFIO_PATH "vfio"

#ifndef VFIO_NOIOMMU_IOMMU
  #define VFIO_NOIOMMU_IOMMU 8
#endif

namespace villas {
namespace kernel {

namespace pci {

/* Forward declarations */
struct Device;

}

namespace vfio {

/* Forward declarations */
class Container;
class Group;

class Device {
	friend class Container;
public:
	Device(const std::string &name, Group &group) :
	    name(name), group(group) {}

	~Device();

	bool reset();

	/** Map a device memory region to the application address space (e.g. PCI BARs) */
	void* regionMap(size_t index);

	/** munmap() a region which has been mapped by vfio_map_region() */
	bool regionUnmap(size_t index);

	/** Get the size of a device memory region */
	size_t regionGetSize(size_t index);


	/** Enable memory accesses and bus mastering for PCI device */
	bool pciEnable();

	bool pciHotReset();
	int pciMsiInit(int efds[32]);
	int pciMsiDeinit(int efds[32]);
	bool pciMsiFind(int nos[32]);

	bool isVfioPciDevice() const;

private:
	/// Name of the device as listed under
	/// /sys/kernel/iommu_groups/[vfio_group::index]/devices/
	std::string name;

	/// VFIO device file descriptor
	int fd;

	struct vfio_device_info info;

	std::vector<struct vfio_irq_info> irqs;
	std::vector<struct vfio_region_info> regions;
	std::vector<void*> mappings;

	/**< libpci handle of the device */
	const kernel::pci::Device *pci_device;

	Group &group;		/**< The VFIO group this device belongs to */
};



class Group {
	friend class Container;
	friend Device;
private:
	Group(int index) : fd(-1), index(index) {}
public:
	~Group();

	static std::unique_ptr<Group>
	attach(Container &container, int groupIndex);

private:
	/// VFIO group file descriptor
	int fd;

	/// Index of the IOMMU group as listed under /sys/kernel/iommu_groups/
	int index;

	/// Status of group
	struct vfio_group_status status;

	/// All devices owned by this group
	std::list<std::unique_ptr<Device>> devices;

	Container* container;	/**< The VFIO container to which this group is belonging */
};


class Container {
private:
	Container();
public:
	~Container();

	static std::shared_ptr<Container>
	create();

	void dump();

	Device & attachDevice(const char *name, int groupIndex);
	Device & attachDevice(const pci::Device &pdev);

	/**
	 * @brief Map VM to an IOVA, which is accessible by devices in the container
	 * @param virt		virtual address of memory
	 * @param phys		IOVA where to map @p virt, -1 to use VFIO internal allocator
	 * @param length	size of memory region in bytes
	 * @return			IOVA address, UINTPTR_MAX on failure
	 */
	uintptr_t memoryMap(uintptr_t virt, uintptr_t phys, size_t length);

	/** munmap() a region which has been mapped by vfio_map_region() */
	bool memoryUnmap(uintptr_t phys, size_t length);

	bool isIommuEnabled() const
	{ return this->hasIommu; }

	const int &getFd() const
	{ return fd; }

private:
	Group & getOrAttachGroup(int index);

private:
	int fd;
	int version;
	int extensions;
	uint64_t iova_next;			/**< Next free IOVA address */
	bool hasIommu;

	/// All groups bound to this container
	std::list<std::unique_ptr<Group>> groups;
};

/** @} */

} /* namespace vfio */
} /* namespace kernel */
} /* namespace villas */
