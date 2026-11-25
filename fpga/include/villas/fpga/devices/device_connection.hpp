/* Vfio connection to a device.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2024-25 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <villas/fpga/devices/device.hpp>
#include <villas/fpga/vfio/container.hpp>
#include <villas/fpga/vfio/device.hpp>

namespace villas {
namespace fpga {
namespace devices {

class DeviceConnection {
public:
  Logger logger;
  const std::shared_ptr<vfio::Device> vfio_device;

private:
  DeviceConnection(std::shared_ptr<vfio::Device> vfio_device);

public:
  static DeviceConnection from(const villas::fpga::devices::Device &device,
                               std::shared_ptr<vfio::Container> vfio_container);

  void addToMemorygraph() const;
};

} // namespace devices
} // namespace fpga
} // namespace villas
