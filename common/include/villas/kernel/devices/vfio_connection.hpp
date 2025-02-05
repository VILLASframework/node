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

namespace villas {
namespace kernel {
namespace devices {

class VfioConnection {
public:
  const std::shared_ptr<kernel::vfio::Device> vfio_device;

private:
  VfioConnection(std::shared_ptr<kernel::vfio::Device> vfio_device)
      : vfio_device(vfio_device){};

public:
  static VfioConnection
  from(villas::kernel::devices::IpDevice ip_device,
       std::shared_ptr<kernel::vfio::Container> vfio_container) {
    auto logger = villas::Log::get("Builder: VfioConnection");

    // Attach group to container
    const int iommu_group = ip_device.iommu_group().value();
    auto vfio_group = vfio_container->getOrAttachGroup(iommu_group);
    logger->debug("Device: {}, Iommu: {}", ip_device.name(), iommu_group);

    // Open Vfio Device
    auto vfio_device = std::make_shared<kernel::vfio::Device>(
        ip_device.name(), vfio_group->getFileDescriptor());

    // Attach device to group
    vfio_group->attachDevice(vfio_device);

    return VfioConnection(vfio_device);
  }
};

} // namespace devices
} // namespace kernel
} // namespace villas