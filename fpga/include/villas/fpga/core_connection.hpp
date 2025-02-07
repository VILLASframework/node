/* Memorygraph-connection to a fpga ip core.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2024-25 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <algorithm>
#include <memory>

#include <villas/fpga/core.hpp>
#include <villas/kernel/devices/device_connection.hpp>
#include <villas/kernel/devices/ip_device.hpp>

namespace villas {
namespace fpga {

class CoreConnection {
public:
  const Logger logger;
  const std::shared_ptr<villas::fpga::ip::Core> ip;
  const villas::kernel::devices::DeviceConnection device_connection;

private:
  CoreConnection(std::shared_ptr<villas::fpga::ip::Core> ip,
                 villas::kernel::devices::DeviceConnection device_connection)
      : logger(villas::Log::get("CoreConnection")), ip(ip),
        device_connection(device_connection){};

public:
  static CoreConnection
  from(std::shared_ptr<villas::fpga::ip::Core> ip,
       std::shared_ptr<kernel::vfio::Container> vfio_container) {
    // Find matching dtr device for ip
    const std::filesystem::path PLATFORM_DEVICES_DIRECTORY(
        "/sys/bus/platform/devices");
    std::vector<villas::kernel::devices::IpDevice> devices =
        villas::kernel::devices::IpDevice::from_directory(
            PLATFORM_DEVICES_DIRECTORY);

    auto it_device =
        std::find_if(devices.begin(), devices.end(),
                     [ip](villas::kernel::devices::IpDevice device) {
                       return ip->getBaseaddr() == device.addr();
                     });

    if (it_device == devices.end()) {
      throw(std::runtime_error(
          "No device found with dtr adress matching ip baseadress."));
    }

    return CoreConnection(ip, villas::kernel::devices::DeviceConnection::from(
                                  *it_device, vfio_container));
  }

  void add_to_memorygraph() {
    device_connection.add_to_memorygraph();

    for (std::string memory_block : ip->getMemoryBlocks()) {
      auto &mm = MemoryManager::get();
      const size_t ip_mem_size = 65536;

      size_t srcVertexId =
          mm.findAddressSpace(device_connection.vfio_device->getName());

      std::string taget_address_space_name =
          ip->getInstanceName() + "/" + memory_block;
      size_t targetVertexId;
      targetVertexId = mm.findAddressSpace(taget_address_space_name);

      mm.createMapping(0, 0, ip_mem_size, "vfio to ip", srcVertexId,
                       targetVertexId);

      logger->debug("Connect {} and {}",
                    mm.getGraph().getVertex(srcVertexId)->name,
                    mm.getGraph().getVertex(targetVertexId)->name);
    }
  }
};

} // namespace fpga
} // namespace villas