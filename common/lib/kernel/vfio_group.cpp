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
#include <villas/kernel/vfio_group.hpp>

using namespace villas::kernel::vfio;

Group::Group(int index, bool iommuEnabled) :
	fd(-1),
	index(index),
	attachedToContainer(false),
	status(),
	devices(),
	log(logging.get("kernel:vfio::Group"))
{
	/* Open group fd */
	std::stringstream groupPath;
	groupPath << VFIO_PATH
	          << (iommuEnabled ? "" : "noiommu-")
	          << index;

	log->debug("path: {}", groupPath.str().c_str());
	fd = open(groupPath.str().c_str(), O_RDWR);
	if (fd < 0) {
		log->error("Failed to open VFIO group {}", index);
		throw RuntimeError("Failed to open VFIO group");
	}

	log->debug("VFIO group {} (fd {}) has path {}",
	              index, fd, groupPath.str());

}

std::shared_ptr<Device> Group::attachDevice(std::shared_ptr<Device> device)
{
	if (device->isAttachedToGroup())
		throw RuntimeError("Device is already attached to a group");

	devices.push_back(device);

	device->setAttachedToGroup();

	return device;
}

std::shared_ptr<Device> Group::attachDevice(const std::string& name, const kernel::pci::Device *pci_device)
{
	auto device = std::make_shared<Device>(name, fd, pci_device);
	return attachDevice(device);
}


bool Group::checkStatus()
{
	int ret;
	if (!attachedToContainer) {
		log->error("Group {} is not attached to a container", index);
		return false;
	}

	/* Check group viability and features */
	status.argsz = sizeof(status);

	ret = ioctl(fd, VFIO_GROUP_GET_STATUS, &status);
	if (ret < 0) {
		log->error("Failed to get VFIO group status");
		return false;
	}

	if (!(status.flags & VFIO_GROUP_FLAGS_VIABLE)) {
		log->error("VFIO group is not available: bind all devices to the VFIO driver!");
		return false;
	}
	return true;
}

void Group::dump()
{
	log->info("VFIO Group {}, viable={}, container={}",
		index,
		(status.flags & VFIO_GROUP_FLAGS_VIABLE) > 0,
		(status.flags & VFIO_GROUP_FLAGS_CONTAINER_SET) > 0
	);

	for (auto& device : devices) {
		device->dump();
	}
}

Group::~Group()
{
	log->debug("Clean up group {} with fd {}", this->index, this->fd);

	/* Release memory and close fds */
	devices.clear();

	if (fd < 0)
		log->debug("Destructing group that has not been attached");
	else {
		log->debug("unsetting group container");
		int ret = ioctl(fd, VFIO_GROUP_UNSET_CONTAINER);
		if (ret != 0)
			log->error("Cannot unset container for group fd {}", fd);

		ret = close(fd);
		if (ret != 0)
			log->error("Cannot close group fd {}", fd);
	}
}
