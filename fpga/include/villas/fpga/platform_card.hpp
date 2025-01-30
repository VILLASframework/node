/* Platform based card
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-FileCopyrightText: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <filesystem>
#include <vector>
#include <villas/fpga/card.hpp>
#include <villas/kernel/devices/ip_device.hpp>

namespace villas {
namespace fpga {

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
    auto logger = villas::Log::get("Static VfioConnection");

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

class CoreConnection {
public:
  const VfioConnection vfio_connection;
  const std::shared_ptr<ip::Core> ip;

  CoreConnection(VfioConnection vfio_connection, std::shared_ptr<ip::Core> ip)
      : vfio_connection(vfio_connection), ip(ip){};
};

class PlatformCard : public Card {
public:
  PlatformCard(std::shared_ptr<kernel::vfio::Container> vfioContainer);

  ~PlatformCard(){};

  std::vector<CoreConnection> core_connections;

  void
  connectVFIOtoIps(std::list<std::shared_ptr<ip::Core>> configuredIps) override;
  bool mapMemoryBlock(const std::shared_ptr<MemoryBlock> block) override;

private:
  void connect(std::string device_name, std::shared_ptr<ip::Core> ip);
};

class PlatformCardFactory {
public:
  static std::shared_ptr<PlatformCard>
  make(json_t *json_card, std::string card_name,
       std::shared_ptr<kernel::vfio::Container> vc,
       const std::filesystem::path &searchPath);
};

} /* namespace fpga */
} /* namespace villas */
