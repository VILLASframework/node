/* AXI-PCIe Interrupt controller
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Steffen Vogel <post@steffenvogel.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <xilinx/xintc.h>

#include <villas/fpga/core.hpp>
#include <villas/kernel/vfio_device.hpp>

namespace villas {
namespace fpga {
namespace ip {

class InterruptController : public Core {
public:
  using IrqMaskType = uint32_t;
  static constexpr int maxIrqs = 32;

  virtual ~InterruptController();

  virtual bool init() override;
  virtual bool stop() override;

  bool enableInterrupt(IrqMaskType mask, bool polling);
  bool enableInterrupt(IrqPort irq, bool polling) {
    return enableInterrupt(1 << irq.num, polling);
  }

  bool disableInterrupt(IrqMaskType mask);
  bool disableInterrupt(IrqPort irq) { return disableInterrupt(1 << irq.num); }

  ssize_t waitForInterrupt(int irq);
  ssize_t waitForInterrupt(IrqPort irq) { return waitForInterrupt(irq.num); }

private:
  static constexpr char registerMemory[] = "reg0";

  std::shared_ptr<villas::kernel::vfio::Device> vfioDevice = nullptr;

  std::list<MemoryBlockName> getMemoryBlocks() const {
    return {registerMemory};
  }

  struct Interrupt {
    int eventFd;  // Event file descriptor
    int number;   // Interrupt number from /proc/interrupts
    bool polling; // Polled or not
  };

  std::vector<kernel::vfio::Device::IrqVectorInfo> irq_vectors;
  int nos[maxIrqs];
  bool polling[maxIrqs];
};

} // namespace ip
} // namespace fpga
} // namespace villas
