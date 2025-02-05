#pragma once

#include <memory>

#include <villas/kernel/devices/vfio_connection.hpp>
#include <villas/fpga/core.hpp>

namespace villas {
namespace fpga {

class CoreConnection {
public:
  const villas::kernel::devices::VfioConnection vfio_connection;
  const std::shared_ptr<villas::fpga::ip::Core> ip;

  CoreConnection(villas::kernel::devices::VfioConnection vfio_connection, std::shared_ptr<villas::fpga::ip::Core> ip)
      : vfio_connection(vfio_connection), ip(ip){};
};

} // namespace fpga
} // namespace villas