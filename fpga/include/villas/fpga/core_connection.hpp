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
                 villas::kernel::devices::DeviceConnection device_connection);

public:
  static CoreConnection
  from(std::shared_ptr<villas::fpga::ip::Core> ip,
       std::shared_ptr<kernel::vfio::Container> vfio_container);

  void add_to_memorygraph();
};

} // namespace fpga
} // namespace villas
