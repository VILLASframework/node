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
#include <string>
#include <vector>

#include <linux/vfio.h>
#include <sys/mman.h>

#include <villas/kernel/devices/pci_device.hpp>
#include <villas/log.hpp>

namespace villas {
namespace kernel {
namespace vfio {

class Device {
public:
  Device(const std::string &name, int groupFileDescriptor,
         const kernel::devices::PciDevice *pci_device = nullptr);
  ~Device();

  // No copying allowed because we manage the vfio state in constructor and destructors
  Device(Device const &) = delete;
  void operator=(Device const &) = delete;

  bool reset();

  std::string getName() { return name; };

  // Map a device memory region to the application address space (e.g. PCI BARs)
  void *regionMap(size_t index);

  // munmap() a region which has been mapped by vfio_map_region()
  bool regionUnmap(size_t index);

  // Get the size of a device memory region
  size_t regionGetSize(size_t index);

  int platformInterruptInit(int efds[32]);

  // Enable memory accesses and bus mastering for PCI device
  bool pciEnable();

  int pciMsiInit(int efds[32]);
  int pciMsiDeinit(int efds[32]);
  bool pciMsiFind(int nos[32]);

  bool isVfioPciDevice() const;
  bool pciHotReset();

  int getFileDescriptor() const { return fd; }
  std::vector<int> &getEventfdList() { return eventfdList; }

  void dump();

  bool isAttachedToGroup() const { return attachedToGroup; }

  void setAttachedToGroup() { this->attachedToGroup = true; }

  int getNumberIrqs() const { return this->info.num_irqs; }

private:
  // Name of the device as listed under
  // /sys/kernel/iommu_groups/[vfio_group::index]/devices/
  std::string name;

  // VFIO device file descriptor
  int fd;

  bool attachedToGroup;
  int groupFd;

  // Interrupt eventfds
  std::vector<int> eventfdList;

  struct vfio_device_info info;

  std::vector<struct vfio_irq_info> irqs;
  std::vector<struct vfio_region_info> regions;
  std::vector<void *> mappings;

  // libpci handle of the device
  const kernel::devices::PciDevice *pci_device;

  Logger log;
};

} // namespace vfio
} // namespace kernel
} // namespace villas
