/* Vfio connection to a device.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2024-25 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <villas/kernel/vfio_device.hpp>
#include <villas/kernel/devices/linux_driver.hpp>

namespace villas {
namespace kernel {
namespace devices {

class DeviceConnection {
public:
  Logger logger;
  const std::shared_ptr<kernel::vfio::Device> vfio_device;

private:
  DeviceConnection(std::shared_ptr<kernel::vfio::Device> vfio_device)
      : logger(villas::Log::get("DeviceConnection")),
        vfio_device(vfio_device){};

public:
  static DeviceConnection
  from(const villas::kernel::devices::Device& device,
       std::shared_ptr<kernel::vfio::Container> vfio_container) {
    auto logger = villas::Log::get("Builder: DeviceConnection");

    // Bind to driver
    LinuxDriver driver(
        std::filesystem::path("/sys/bus/platform/drivers/vfio-platform"));
    driver.attach(device);

    // Attach group to container
    const int iommu_group = device.iommu_group().value();
    auto vfio_group = vfio_container->getOrAttachGroup(iommu_group);
    logger->debug("Device: {}, Iommu: {}", device.name(), iommu_group);

    // Open Vfio Device
    auto vfio_device = std::make_shared<kernel::vfio::Device>(
        device.name(), vfio_group->getFileDescriptor());

    // Attach device to group
    vfio_group->attachDevice(vfio_device);

    return DeviceConnection(vfio_device);
  }

  void add_to_memorygraph() const {
    // Map vfio device memory to process
    const void *mapping = this->vfio_device->regionMap(0);
    if (mapping == MAP_FAILED) {
      logger->error("Failed to mmap() device");
    }
    logger->debug("memory mapped: {}", this->vfio_device->getName());

    // Create memorygraph edge from process to vfio device
    auto &mm = MemoryManager::get();
    size_t srcVertexId = mm.getProcessAddressSpace();
    const size_t mem_size = this->vfio_device->regionGetSize(0);
    size_t targetVertexId =
        mm.getOrCreateAddressSpace(this->vfio_device->getName());
    mm.createMapping(reinterpret_cast<uintptr_t>(mapping), 0, mem_size,
                     "process to vfio", srcVertexId, targetVertexId);
    logger->debug("create edge from process to {}",
                  this->vfio_device->getName());
  }
};

} // namespace devices
} // namespace kernel
} // namespace villas