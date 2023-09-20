/* Virtual Function IO wrapper around kernel API.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2014-2021 Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2018 Daniel Krebs <github@daniel-krebs.net>
 * SPDX-License-Identifier: Apache-2.0
 */

#define _DEFAULT_SOURCE

#if defined(__arm__) || defined(__aarch64__)
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64
#endif

#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
#include <string>

#include <cstdlib>
#include <cstring>

#include <cstdint>
#include <fcntl.h>
#include <linux/pci_regs.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <villas/exceptions.hpp>
#include <villas/kernel/kernel.hpp>
#include <villas/kernel/vfio_container.hpp>
#include <villas/log.hpp>

using namespace villas::kernel::vfio;

#ifndef VFIO_NOIOMMU_IOMMU
#define VFIO_NOIOMMU_IOMMU 8
#endif

static std::array<std::string, EXTENSION_SIZE> construct_vfio_extension_str() {
  std::array<std::string, EXTENSION_SIZE> ret;
  ret[VFIO_TYPE1_IOMMU] = "Type 1";
  ret[VFIO_SPAPR_TCE_IOMMU] = "SPAPR TCE";
  ret[VFIO_TYPE1v2_IOMMU] = "Type 1 v2";
  ret[VFIO_DMA_CC_IOMMU] = "DMA CC";
  ret[VFIO_EEH] = "EEH";
  ret[VFIO_TYPE1_NESTING_IOMMU] = "Type 1 Nesting";
  ret[VFIO_SPAPR_TCE_v2_IOMMU] = "SPAPR TCE v2";
  ret[VFIO_NOIOMMU_IOMMU] = "No IOMMU";
// Backwards compatability with older kernels
#ifdef VFIO_UNMAP_ALL
  ret[VFIO_UNMAP_ALL] = "Unmap all";
#endif
#ifdef VFIO_UPDATE_VADDR
  ret[VFIO_UPDATE_VADDR] = "Update vaddr";
#endif
  return ret;
}

static std::array<std::string, EXTENSION_SIZE> VFIO_EXTENSION_STR =
    construct_vfio_extension_str();

Container::Container(std::vector<std::string> required_modules)
    : fd(-1), version(0), extensions(), iova_next(0), hasIommu(false), groups(),
      log(logging.get("kernel:vfio:container")) {
  for (auto module : required_modules) {
    if (kernel::loadModule(module.c_str()) != 0) {
      throw RuntimeError("Kernel module '{}' required but could not be loaded. "
                         "Please load manually!",
                         module);
    }
  }

  // Create a VFIO Container
  fd = open(VFIO_DEV, O_RDWR);
  if (fd < 0)
    throw RuntimeError("Failed to open VFIO container");

  // Check VFIO API version
  version = ioctl(fd, VFIO_GET_API_VERSION);
  if (version < 0 || version != VFIO_API_VERSION)
    throw RuntimeError("Unknown API version: {}", version);

  // Check available VFIO extensions (IOMMU types)
  for (unsigned int i = VFIO_TYPE1_IOMMU; i < EXTENSION_SIZE; i++) {
    int ret = ioctl(fd, VFIO_CHECK_EXTENSION, i);

    extensions[i] = ret != 0;

    log->debug("VFIO extension {} is {} ({})", i,
               extensions[i] ? "available" : "not available",
               VFIO_EXTENSION_STR[i]);
  }

  if (extensions[VFIO_TYPE1_IOMMU]) {
    log->debug("Using VFIO type {} ({})", VFIO_TYPE1_IOMMU,
               VFIO_EXTENSION_STR[VFIO_TYPE1_IOMMU]);
    hasIommu = true;
  } else if (extensions[VFIO_NOIOMMU_IOMMU]) {
    log->debug("Using VFIO type {} ({})", VFIO_NOIOMMU_IOMMU,
               VFIO_EXTENSION_STR[VFIO_NOIOMMU_IOMMU]);
    hasIommu = false;
  } else
    throw RuntimeError("No supported IOMMU type available");

  log->debug("Version:    {:#x}", version);
  log->debug("IOMMU:      {}", hasIommu ? "yes" : "no");
}

Container::~Container() {
  // Release memory and close fds
  groups.clear();

  log->debug("Cleaning up container with fd {}", fd);

  // Close container
  int ret = close(fd);
  if (ret < 0)
    log->error("Error closing vfio container fd {}: {}", fd, ret);
}

void Container::attachGroup(std::shared_ptr<Group> group) {
  if (group->isAttachedToContainer())
    throw RuntimeError("Group is already attached to a container");

  // Claim group ownership
  int ret = ioctl(group->getFileDescriptor(), VFIO_GROUP_SET_CONTAINER, &fd);
  if (ret < 0)
    throw SystemError("Failed to attach VFIO group {} to container fd {}",
                      group->getIndex(), fd);

  // Set IOMMU type
  int iommu_type = isIommuEnabled() ? VFIO_TYPE1_IOMMU : VFIO_NOIOMMU_IOMMU;

  ret = ioctl(fd, VFIO_SET_IOMMU, iommu_type);
  if (ret < 0)
    throw SystemError("Failed to set IOMMU type of container");

  group->setAttachedToContainer();

  log->debug("Attached new group {} to VFIO container with fd {}",
             group->getIndex(), fd);

  // Push to our list
  groups.push_back(std::move(group));
}

std::shared_ptr<Group> Container::getOrAttachGroup(int index) {
  // Search if group with index already exists
  for (auto &group : groups) {
    if (group->getIndex() == index)
      return group;
  }

  // Group not yet part of this container, so acquire ownership
  auto group = std::make_shared<Group>(index, isIommuEnabled());
  attachGroup(group);

  return group;
}

void Container::dump() {
  log->info("File descriptor: {}", fd);
  log->info("Version: {}", version);

  // Check available VFIO extensions (IOMMU types)
  for (size_t i = 0; i < extensions.size(); i++)
    log->debug("VFIO extension {} ({}) is {}", extensions[i],
               VFIO_EXTENSION_STR[i],
               extensions[i] ? "available" : "not available");

  for (auto &group : groups)
    group->dump();
}

std::shared_ptr<Device> Container::attachDevice(const std::string &name,
                                                int index) {
  auto group = getOrAttachGroup(index);
  auto device = group->attachDevice(name);

  return device;
}

std::shared_ptr<Device> Container::attachDevice(pci::Device &pdev) {
  int ret;
  char name[32], iommu_state[4];
  static constexpr const char *kernelDriver = "vfio-pci";

  // Load PCI bus driver for VFIO
  if (kernel::loadModule("vfio_pci"))
    throw RuntimeError("Failed to load kernel driver: vfio_pci");

  // Bind PCI card to vfio-pci driver if not already bound
  if (pdev.getDriver() != kernelDriver) {
    log->debug("Bind PCI card to kernel driver '{}'", kernelDriver);
    pdev.attachDriver(kernelDriver);
  }

  try {
    pdev.rewriteBar();
  } catch (std::exception &e) {
    throw RuntimeError(
        "BAR of device is in inconsistent state. Rewriting the BAR "
        "failed. Please remove, rescan and reset the device and try again.");
  }

  // Get IOMMU group of device
  int index = isIommuEnabled() ? pdev.getIommuGroup() : 0;
  if (index < 0) {
    ret = kernel::getCmdlineParam("intel_iommu", iommu_state,
                                  sizeof(iommu_state));
    if (ret != 0 || strcmp("on", iommu_state) != 0)
      log->warn("Kernel booted without command line parameter "
                "'intel_iommu' set to 'on'. Please check documentation "
                "(https://villas.fein-aachen.org/doc/fpga-setup.html) "
                "for help with troubleshooting.");

    throw RuntimeError("Failed to get IOMMU group of device");
  }

  // VFIO device name consists of PCI BDF
  snprintf(name, sizeof(name), "%04x:%02x:%02x.%x", pdev.slot.domain,
           pdev.slot.bus, pdev.slot.device, pdev.slot.function);

  log->info("Attach to device {} with index {}", std::string(name), index);
  auto group = getOrAttachGroup(index);
  auto device = group->attachDevice(name, &pdev);

  // Check if this is really a vfio-pci device
  if (!device->isVfioPciDevice())
    throw RuntimeError("Device is not a vfio-pci device");

  return device;
}

uintptr_t Container::memoryMap(uintptr_t virt, uintptr_t phys, size_t length) {
  int ret;

  if (not hasIommu) {
    log->error("DMA mapping not supported without IOMMU");
    return UINTPTR_MAX;
  }

  if (length & 0xFFF) {
    length += 0x1000;
    length &= ~0xFFF;
  }

  // Super stupid allocator
  size_t iovaIncrement = 0;
  if (phys == UINTPTR_MAX) {
    phys = this->iova_next;
    iovaIncrement = length;
  }

  struct vfio_iommu_type1_dma_map dmaMap;
  memset(&dmaMap, 0, sizeof(dmaMap));

  dmaMap.argsz = sizeof(dmaMap);
  dmaMap.vaddr = virt;
  dmaMap.iova = phys;
  dmaMap.size = length;
  dmaMap.flags = VFIO_DMA_MAP_FLAG_READ | VFIO_DMA_MAP_FLAG_WRITE;

  ret = ioctl(this->fd, VFIO_IOMMU_MAP_DMA, &dmaMap);
  if (ret) {
    log->error("Failed to create DMA mapping: {}", ret);
    return UINTPTR_MAX;
  }

  log->debug("DMA map size={:#x}, iova={:#x}, vaddr={:#x}", dmaMap.size,
             dmaMap.iova, dmaMap.vaddr);

  // Mapping successful, advance IOVA allocator
  this->iova_next += iovaIncrement;

  // We intentionally don't return the actual mapped length, the users are
  // only guaranteed to have their demanded memory mapped correctly
  return dmaMap.iova;
}

bool Container::memoryUnmap(uintptr_t phys, size_t length) {
  int ret;

  if (not hasIommu)
    return true;

  struct vfio_iommu_type1_dma_unmap dmaUnmap;
  dmaUnmap.argsz = sizeof(struct vfio_iommu_type1_dma_unmap);
  dmaUnmap.flags = 0;
  dmaUnmap.iova = phys;
  dmaUnmap.size = length;

  ret = ioctl(this->fd, VFIO_IOMMU_UNMAP_DMA, &dmaUnmap);
  if (ret) {
    log->error("Failed to unmap DMA mapping");
    return false;
  }
  log->debug("DMA unmap size={:#x}, iova={:#x}", dmaUnmap.size, dmaUnmap.iova);

  return true;
}
