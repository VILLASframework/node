/* Memorygraph-connection to a fpga ip core.
 * This class can make the memory of an ip accessible via the memorygraph by:
 * 1. Finding the corresponding device in the OS-devicetree
 * 2. Binding/Unbinding the required Drivers
 * 3. Opening a Vfio-Connection
 * 4. Adding a vertex and edge to the memorygraph which is connected to process memory
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2024-2025 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/fpga/core_connection.hpp>

namespace villas {
namespace fpga {

CoreConnection::CoreConnection(
    std::shared_ptr<villas::fpga::ip::Core> ip,
    villas::kernel::devices::DeviceConnection device_connection)
    : logger(villas::Log::get("CoreConnection")), ip(ip),
      device_connection(device_connection) {};

CoreConnection
CoreConnection::from(std::shared_ptr<villas::fpga::ip::Core> ip,
                     std::shared_ptr<kernel::vfio::Container> vfio_container) {
  // Find matching OS device for ip
  const fs::path PLATFORM_DEVICES_DIRECTORY("/sys/bus/platform/devices");
  std::vector<villas::kernel::devices::IpDevice> devices =
      villas::kernel::devices::IpDevice::from_directory(
          PLATFORM_DEVICES_DIRECTORY);

  auto it_device = std::find_if(devices.begin(), devices.end(),
                                [ip](villas::kernel::devices::IpDevice device) {
                                  return ip->getBaseaddr() == device.addr();
                                });

  if (it_device == devices.end()) {
    throw(std::runtime_error(
        "No device found with address matching ip baseadress."));
  }

  return CoreConnection(ip, villas::kernel::devices::DeviceConnection::from(
                                *it_device, vfio_container));
}

void CoreConnection::add_to_memorygraph() {
  device_connection.addToMemorygraph();

  for (std::string memory_block : ip->getMemoryBlocks()) {
    auto &mm = MemoryManager::get();
    // 16 bit width => 2^16 = 65536
    const size_t ip_mem_size = 65536;

    size_t srcVertexId =
        mm.findAddressSpace(device_connection.vfio_device->getName());

    std::string taget_address_space_name =
        ip->getInstanceName() + "/" + memory_block;
    size_t targetVertexId;
    targetVertexId = mm.findAddressSpace(taget_address_space_name);

    // Translation 0,0: No translation needs to be done to host vfio.
    mm.createMapping(0, 0, ip_mem_size, "vfio to ip", srcVertexId,
                     targetVertexId);

    logger->debug("Connect {} and {}",
                  mm.getGraph().getVertex(srcVertexId)->name,
                  mm.getGraph().getVertex(targetVertexId)->name);
  }
}

} // namespace fpga
} // namespace villas
