/* Vfio connection to a device.
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2024-25 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <villas/kernel/devices/device.hpp>
#include <villas/kernel/vfio_container.hpp>
#include <villas/kernel/vfio_device.hpp>

namespace villas {
namespace kernel {
namespace devices {

class DeviceConnection {
public:
  Logger logger;
  const std::shared_ptr<kernel::vfio::Device> vfio_device;

private:
  DeviceConnection(std::shared_ptr<kernel::vfio::Device> vfio_device);

public:
  static DeviceConnection
  from(const villas::kernel::devices::Device &device,
       std::shared_ptr<kernel::vfio::Container> vfio_container);

  void addToMemorygraph() const;
};

} // namespace devices
} // namespace kernel
} // namespace villas
