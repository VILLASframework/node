#pragma once

#include <villas/fpga/ips/intc.hpp>
#include <villas/kernel/vfio_device.hpp>

class PlatformInterruptController
    : public villas::fpga::ip::InterruptController {
public:
  PlatformInterruptController(
      std::shared_ptr<villas::kernel::vfio::Device> vfioDevice) {
    //TODO: efds
    this->num_irqs = vfioDevice->platformInterruptInit(efds);
  }

  bool enableInterrupt(IrqMaskType mask, bool polling) override {
    for (int i = 0; i < num_irqs; i++) {
      if (mask & (1 << i))
        this->polling[i] = polling;
    }
    return true;
  }
};