/* Platform Interrupt Controller
 *
 * Author: Pascal Bauer <pascal.bauer@rwth-aachen.de>
 *
 * SPDX-FileCopyrightText: 2023-2024 Pascal Bauer <pascal.bauer@rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <exception>
#include <memory>
#include <stdexcept>
#include <villas/fpga/card/platform_card.hpp>
#include <villas/fpga/ips/platform_intc.hpp>

bool PlatformInterruptController::init() {
  auto platform_card = dynamic_cast<villas::fpga::PlatformCard *>(card);

  std::shared_ptr<villas::kernel::vfio::Device> dma_vfio_device;
  for (auto device : platform_card->devices) {
    if (device->getName().find("dma") != std::string::npos) {
      dma_vfio_device = device;
      break;
    }
  }

  if (dma_vfio_device == nullptr)
    throw std::logic_error("No vfio device with name containing dma found.");

  dma_vfio_device->platformInterruptInit(efds);
  return true;
}

bool PlatformInterruptController::enableInterrupt(
    InterruptController::IrqMaskType mask, bool polling) {
  logger->debug("Enabling Platform Interrupt");
  for (int i = 0; i < num_irqs; i++) {
    if (mask & (1 << i))
      this->polling[i] = polling;
  }
  return true;
}

bool PlatformInterruptController::enableInterrupt(IrqPort irq, bool polling) {
  logger->debug("Enabling Platform Interrupt");
  this->polling[irq.num] = polling;
  return true;
}
