/* Virtual Function IO wrapper around kernel API.
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2022-2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2018 Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <list>
#include <memory>
#include <vector>

#include <linux/vfio.h>
#include <sys/mman.h>

#include <villas/kernel/vfio_device.hpp>
#include <villas/log.hpp>

namespace villas {
namespace kernel {
namespace vfio {

#define VFIO_PATH "/dev/vfio/"
#define VFIO_DEV VFIO_PATH "vfio"

class Group {
public:
  Group(int index, bool iommuEnabled);
  ~Group();

  // No copying allowed because we manage the vfio state in constructor and destructors
  Group(Group const &) = delete;
  void operator=(Group const &) = delete;

  void setAttachedToContainer() { attachedToContainer = true; }

  bool isAttachedToContainer() { return attachedToContainer; }

  int getFileDescriptor() { return fd; }

  int getIndex() { return index; }

  std::shared_ptr<Device> attachDevice(std::shared_ptr<Device> device);
  std::shared_ptr<Device>
  attachDevice(const std::string &name,
               const kernel::pci::Device *pci_device = nullptr);

  bool checkStatus();
  void dump();

private:
  // VFIO group file descriptor
  int fd;

  // Index of the IOMMU group as listed under /sys/kernel/iommu_groups/
  int index;

  bool attachedToContainer;

  // Status of group
  struct vfio_group_status status;

  // All devices owned by this group
  std::list<std::shared_ptr<Device>> devices;

  Logger log;
};

} // namespace vfio
} // namespace kernel
} // namespace villas
