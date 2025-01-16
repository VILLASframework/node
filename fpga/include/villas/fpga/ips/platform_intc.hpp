#pragma once

#include <villas/fpga/ips/intc.hpp>
#include <villas/fpga/platform_card.hpp>
#include <villas/kernel/vfio_device.hpp>

namespace villas {
namespace fpga {
namespace ip {

class PlatformInterruptController
    : public villas::fpga::ip::InterruptController {
public:
  std::shared_ptr<villas::fpga::ip::Core> core;

  PlatformInterruptController(std::shared_ptr<villas::fpga::ip::Core> core)
      : core(core) {
    logger = Log::get("PlatformIntc");
    this->id = IpIdentifier("acs:software:platformIntc:1.0",
                            "PlatformInterruptcontroller");
  };

  std::list<MemoryBlockName> getMemoryBlocks() const override { return {}; }

  bool init() override {
    std::shared_ptr<villas::kernel::vfio::Device> vfio_device;
    auto platform_card = dynamic_cast<villas::fpga::PlatformCard *>(card);
    for (auto core_connection : platform_card->core_connections) {
      if (core_connection.ip->getBaseaddr() == core->getBaseaddr()) {
        vfio_device = core_connection.vfio_connection.vfio_device;
        logger->info("Enabled VFIO-Interrupt for {} with addr {:#x} ",
                     core->getInstanceName(), core->getBaseaddr());
      }
    }

    if (vfio_device == nullptr) {
      logger->warn("Could not find Vfio device for ip: {}",
                   core->getInstanceName());
      return -1;
    }

    this->num_irqs = vfio_device->platformInterruptInit(efds);

    return true;
  };

  bool enableInterrupt(IrqMaskType mask, bool polling) override {
    logger->debug("Enabling interrupt (platform)");
    for (int i = 0; i < num_irqs; i++) {
      if (mask & (1 << i))
        this->polling[i] = polling;
    }
    return true;
  }
};

} // namespace ip
} // namespace fpga
} // namespace villas
