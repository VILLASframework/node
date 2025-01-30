#pragma once

#include <sys/ioctl.h>
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
  std::shared_ptr<villas::kernel::vfio::Device> vfio_device = nullptr;

  PlatformInterruptController(std::shared_ptr<villas::fpga::ip::Core> core)
      : core(core) {
    logger = Log::get("PlatformIntc");
    this->id = IpIdentifier("acs:software:platformIntc:1.0",
                            "PlatformInterruptcontroller");
  };

  std::list<MemoryBlockName> getMemoryBlocks() const override { return {}; }

  bool init() override {
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

  ssize_t waitForInterrupt(int irq) override {
    assert(irq < maxIrqs);
    assert(irq < this->num_irqs);

    uint64_t count;
    int sret;
    fd_set rfds;
    struct timeval tv = {.tv_sec = 1, .tv_usec = 0};
    FD_ZERO(&rfds);
    FD_SET(efds[irq], &rfds);
    logger->debug("Waiting for interrupt fd {}", efds[irq]);

    sret = select(efds[irq] + 1, &rfds, NULL, NULL, &tv);
    if (sret == -1) {
      logger->error("select() failed: {}", strerror(errno));
      return -1;
    } else if (sret == 0) {
      logger->warn("timeout waiting for interrupt {}", irq);

      return -1;
    }
    // Block until there has been an interrupt, read number of interrupts
    ssize_t ret = read(efds[irq], &count, sizeof(count));
    if (ret != sizeof(count)) {
      logger->error("Failed to read from eventfd: {}", strerror(errno));
      return -1;
    }

    struct vfio_irq_set irqSet = {0};
    irqSet.argsz = sizeof(struct vfio_irq_set);
    irqSet.flags = (VFIO_IRQ_SET_DATA_NONE |
                    VFIO_IRQ_SET_ACTION_UNMASK); // TODO: Change based on mask
    irqSet.index = irq; //TODO: Set index
    irqSet.start = 0;
    irqSet.count = 1;
    ret = ioctl(this->vfio_device->getFileDescriptor(), VFIO_DEVICE_SET_IRQS,
                &irqSet);
    if (ret < 0) {
      logger->error("Failed to unmask IRQ {}", 0);
    }

    return count;
  }
};

} // namespace ip
} // namespace fpga
} // namespace villas
