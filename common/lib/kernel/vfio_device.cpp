/* Virtual Function IO wrapper around kernel API.
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2022-2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2018 Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#define _DEFAULT_SOURCE

#if defined(__arm__) || defined(__aarch64__)
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64
#endif

#include <algorithm>
#include <limits>
#include <sstream>
#include <string>

#include <fcntl.h>
#include <linux/pci_regs.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <villas/exceptions.hpp>
#include <villas/kernel/vfio_device.hpp>

using namespace villas::kernel::vfio;

static const char *vfio_pci_region_names[] = {
    "PCI_BAR0",   // VFIO_PCI_BAR0_REGION_INDEX
    "PCI_BAR1",   // VFIO_PCI_BAR1_REGION_INDEX
    "PCI_BAR2",   // VFIO_PCI_BAR2_REGION_INDEX
    "PCI_BAR3",   // VFIO_PCI_BAR3_REGION_INDEX
    "PCI_BAR4",   // VFIO_PCI_BAR4_REGION_INDEX
    "PCI_BAR5",   // VFIO_PCI_BAR5_REGION_INDEX
    "PCI_ROM",    // VFIO_PCI_ROM_REGION_INDEX
    "PCI_CONFIG", // VFIO_PCI_CONFIG_REGION_INDEX
    "PCI_VGA"     // VFIO_PCI_INTX_IRQ_INDEX
};

static const char *vfio_pci_irq_names[] = {
    "PCI_INTX", // VFIO_PCI_INTX_IRQ_INDEX
    "PCI_MSI",  // VFIO_PCI_MSI_IRQ_INDEX
    "PCI_MSIX", // VFIO_PCI_MSIX_IRQ_INDEX
    "PCI_ERR",  // VFIO_PCI_ERR_IRQ_INDEX
    "PCI_REQ"   // VFIO_PCI_REQ_IRQ_INDEX
};

Device::Device(const std::string &name, int groupFileDescriptor,
               const kernel::pci::Device *pci_device)
    : name(name), fd(-1), attachedToGroup(false), groupFd(groupFileDescriptor),
      info(), irqs(), regions(), mappings(), pci_device(pci_device),
      log(logging.get("kernel:vfio:device")) {
  if (groupFileDescriptor < 0)
    throw RuntimeError("Invalid group file descriptor");

  // Open device fd
  fd = ioctl(groupFileDescriptor, VFIO_GROUP_GET_DEVICE_FD, name.c_str());
  if (fd < 0)
    throw RuntimeError("Failed to open VFIO device: {}", name.c_str());

  // Get device info
  info.argsz = sizeof(info);

  int ret = ioctl(fd, VFIO_DEVICE_GET_INFO, &info);
  if (ret < 0)
    throw RuntimeError("Failed to get VFIO device info for: {}", name);

  log->debug("device info: flags: 0x{:x}, num_regions: {}, num_irqs: {}",
             info.flags, info.num_regions, info.num_irqs);

  // device_info.num_region reports always 9 and includes a VGA region, which is only supported on
  // certain device IDs. So for non-VGA devices VFIO_PCI_CONFIG_REGION_INDEX will be the highest
  // region index. This is the config space.
  info.num_regions = pci_device != 0 ? VFIO_PCI_CONFIG_REGION_INDEX + 1 : 1;

  // Reserve slots already so that we can use the []-operator for access
  irqs.resize(info.num_irqs);
  regions.resize(info.num_regions);
  mappings.resize(info.num_regions);

  // Get device regions
  for (size_t i = 0; i < info.num_regions; i++) {
    struct vfio_region_info region;
    memset(&region, 0, sizeof(region));

    region.argsz = sizeof(region);
    region.index = i;

    ret = ioctl(fd, VFIO_DEVICE_GET_REGION_INFO, &region);
    if (ret < 0)
      throw RuntimeError("Failed to get region {} of VFIO device: {}", i, name);

    log->debug("region {} info: flags: 0x{:x}, cap_offset: 0x{:x}, size: "
               "0x{:x}, offset: 0x{:x}",
               region.index, region.flags, region.cap_offset, region.size,
               region.offset);

    regions[i] = region;
  }

  // Get device IRQs
  for (size_t i = 0; i < info.num_irqs; i++) {
    struct vfio_irq_info irq;
    memset(&irq, 0, sizeof(irq));

    irq.argsz = sizeof(irq);
    irq.index = i;

    ret = ioctl(fd, VFIO_DEVICE_GET_IRQ_INFO, &irq);
    if (ret < 0)
      throw RuntimeError("Failed to get IRQ {} of VFIO device: {}", i, name);

    log->debug("irq {} info: flags: 0x{:x}, count: {}", irq.index, irq.flags,
               irq.count);

    irqs[i] = irq;
  }
}

Device::~Device() {
  log->debug("Cleaning up device {} with fd {}", this->name, this->fd);

  for (auto &region : regions) {
    regionUnmap(region.index);
  }
  if (isVfioPciDevice()) {
    pciHotReset();
  }
  reset();

  int ret = close(fd);
  if (ret != 0) {
    log->error("Closing device fd {} failed", fd);
  }
}

bool Device::reset() {
  log->debug("Resetting device.");
  if (this->info.flags & VFIO_DEVICE_FLAGS_RESET)
    return ioctl(this->fd, VFIO_DEVICE_RESET) == 0;
  else
    return false; // Not supported by this device
}

void *Device::regionMap(size_t index) {
  struct vfio_region_info *r = &regions[index];

  if (!(r->flags & VFIO_REGION_INFO_FLAG_MMAP))
    return MAP_FAILED;

  int flags = MAP_SHARED;

#if !(defined(__arm__) || defined(__aarch64__))
  flags |= MAP_SHARED | MAP_32BIT;
#endif

  log->debug("Mapping region {} of size 0x{:x} with flags 0x{:x}", index,
             r->size, flags);
  mappings[index] =
      mmap(nullptr, r->size, PROT_READ | PROT_WRITE, flags, fd, r->offset);

  return mappings[index];
}

bool Device::regionUnmap(size_t index) {
  int ret;
  struct vfio_region_info *r = &regions[index];

  if (!mappings[index])
    return false; // Was not mapped

  log->debug("Unmap region {} from device {}", index, name);

  ret = munmap(mappings[index], r->size);
  if (ret)
    return false;

  mappings[index] = nullptr;

  return true;
}

size_t Device::regionGetSize(size_t index) {
  if (index >= regions.size()) {
    log->error("Index out of range: {} >= {}", index, regions.size());
    throw std::out_of_range("Index out of range");
  }

  return regions[index].size;
}

void Device::dump() {
  log->info("Device {}: regions={}, irqs={}, flags={}", name, info.num_regions,
            info.num_irqs, info.flags);

  for (size_t i = 0; i < info.num_regions && i < 8; i++) {
    struct vfio_region_info *region = &regions[i];

    if (region->size > 0) {
      log->info("Region {} {}: size={}, offset={}, flags={}", region->index,
                (info.flags & VFIO_DEVICE_FLAGS_PCI) ? vfio_pci_region_names[i]
                                                     : "",
                region->size, region->offset, region->flags);
    }
  }

  for (size_t i = 0; i < info.num_irqs; i++) {
    struct vfio_irq_info *irq = &irqs[i];

    if (irq->count > 0) {
      log->info("IRQ {} {}: count={}, flags={}", irq->index,
                (info.flags & VFIO_DEVICE_FLAGS_PCI) ? vfio_pci_irq_names[i]
                                                     : "",
                irq->count, irq->flags);
    }
  }
}

bool Device::pciEnable() {
  int ret;
  uint32_t reg;
  const off64_t offset =
      PCI_COMMAND + (static_cast<off64_t>(VFIO_PCI_CONFIG_REGION_INDEX) << 40);

  // Check if this is really a vfio-pci device
  if (!(this->info.flags & VFIO_DEVICE_FLAGS_PCI))
    return false;

  ret = pread64(this->fd, &reg, sizeof(reg), offset);
  if (ret != sizeof(reg))
    return false;

  // Enable memory access and PCI bus mastering which is required for DMA
  reg |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER;

  ret = pwrite64(this->fd, &reg, sizeof(reg), offset);
  if (ret != sizeof(reg))
    return false;

  return true;
}

int Device::pciMsiInit(int efds[]) {
  // Check if this is really a vfio-pci device
  if (not isVfioPciDevice())
    return -1;

  const size_t irqCount = irqs[VFIO_PCI_MSI_IRQ_INDEX].count;
  const size_t irqSetSize =
      sizeof(struct vfio_irq_set) + sizeof(int) * irqCount;

  auto *irqSetBuf = new char[irqSetSize];
  if (!irqSetBuf)
    throw MemoryAllocationError();

  auto *irqSet = reinterpret_cast<struct vfio_irq_set *>(irqSetBuf);

  irqSet->argsz = irqSetSize;
  // DATA_EVENTFD binds the interrupt to the provided eventfd.
  // SET_ACTION_TRIGGER enables kernel->userspace signalling.
  irqSet->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
  irqSet->index = VFIO_PCI_MSI_IRQ_INDEX;
  irqSet->start = 0;
  irqSet->count = irqCount;

  // Now set the new eventfds
  for (size_t i = 0; i < irqCount; i++) {
    efds[i] = eventfd(0, 0);
    if (efds[i] < 0) {
      delete[] irqSetBuf;
      return -1;
    }
    eventfdList.push_back(efds[i]);
  }

  memcpy(irqSet->data, efds, sizeof(int) * irqCount);

  if (ioctl(fd, VFIO_DEVICE_SET_IRQS, irqSet) != 0) {
    delete[] irqSetBuf;
    return -1;
  }

  delete[] irqSetBuf;

  return irqCount;
}

int Device::pciMsiDeinit(int efds[]) {
  // Check if this is really a vfio-pci device
  if (not isVfioPciDevice())
    return -1;

  const size_t irqCount = irqs[VFIO_PCI_MSI_IRQ_INDEX].count;
  const size_t irqSetSize =
      sizeof(struct vfio_irq_set) + sizeof(int) * irqCount;

  auto *irqSetBuf = new char[irqSetSize];
  if (!irqSetBuf)
    throw MemoryAllocationError();

  auto *irqSet = reinterpret_cast<struct vfio_irq_set *>(irqSetBuf);

  irqSet->argsz = irqSetSize;
  irqSet->flags = VFIO_IRQ_SET_DATA_EVENTFD | VFIO_IRQ_SET_ACTION_TRIGGER;
  irqSet->index = VFIO_PCI_MSI_IRQ_INDEX;
  irqSet->count = irqCount;
  irqSet->start = 0;

  for (size_t i = 0; i < irqCount; i++) {
    close(efds[i]);
    efds[i] = -1;
  }

  memcpy(irqSet->data, efds, sizeof(int) * irqCount);

  if (ioctl(fd, VFIO_DEVICE_SET_IRQS, irqSet) != 0) {
    delete[] irqSetBuf;
    return -1;
  }

  delete[] irqSetBuf;

  return irqCount;
}

bool Device::pciMsiFind(int nos[]) {
  int ret, idx, irq;
  char *end, *col, *last, line[1024], name[13];
  FILE *f;

  f = fopen("/proc/interrupts", "r");
  if (!f)
    return false;

  for (int i = 0; i < 32; i++)
    nos[i] = -1;

  // For each line in /proc/interrupts
  while (fgets(line, sizeof(line), f)) {
    col = strtok(line, " ");

    // IRQ number is in first column
    irq = strtol(col, &end, 10);
    if (col == end)
      continue;

    // Find last column of line
    do {
      last = col;
    } while ((col = strtok(nullptr, " ")));

    ret = sscanf(last, "vfio-msi[%d](%12[0-9:])", &idx, name);
    if (ret == 2) {
      if (strstr(this->name.c_str(), name) == this->name.c_str())
        nos[idx] = irq;
    }
  }

  fclose(f);

  return true;
}

bool Device::isVfioPciDevice() const {
  return info.flags & VFIO_DEVICE_FLAGS_PCI;
}

bool Device::pciHotReset() {
  // Check if this is really a vfio-pci device
  if (!isVfioPciDevice())
    return false;

  log->debug("Performing hot reset.");
  const size_t reset_info_len = sizeof(struct vfio_pci_hot_reset_info) +
                                sizeof(struct vfio_pci_dependent_device) * 64;

  auto *reset_info_buf = new char[reset_info_len];
  if (!reset_info_buf)
    throw MemoryAllocationError();

  auto *reset_info =
      reinterpret_cast<struct vfio_pci_hot_reset_info *>(reset_info_buf);

  reset_info->argsz = reset_info_len;

  if (ioctl(fd, VFIO_DEVICE_GET_PCI_HOT_RESET_INFO, reset_info) != 0) {
    delete[] reset_info_buf;
    return false;
  }

  log->debug("Dependent devices for hot-reset:");
  for (size_t i = 0; i < reset_info->count; i++) {
    struct vfio_pci_dependent_device *dd = &reset_info->devices[i];
    log->debug("  {:04x}:{:02x}:{:02x}.{:01x}: iommu_group={}", dd->segment,
               dd->bus, PCI_SLOT(dd->devfn), PCI_FUNC(dd->devfn), dd->group_id);
  }

  delete[] reset_info_buf;

  const size_t reset_len =
      sizeof(struct vfio_pci_hot_reset) + sizeof(int32_t) * 1;
  auto *reset_buf = new char[reset_len];
  if (!reset_buf)
    throw MemoryAllocationError();

  auto *reset = reinterpret_cast<struct vfio_pci_hot_reset *>(reset_buf);

  reset->argsz = reset_len;
  reset->count = 1;
  reset->group_fds[0] = groupFd;

  int ret = ioctl(fd, VFIO_DEVICE_PCI_HOT_RESET, reset);
  const bool success = (ret == 0);

  delete[] reset_buf;

  if (!success) {
    log->warn("PCI hot reset failed, maybe no IOMMU available?");
    return true;
  }

  return success;
}
