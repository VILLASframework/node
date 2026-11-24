/* Linux PCI helpers.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <list>

#include <villas/log.hpp>

namespace villas {
namespace kernel {
namespace devices {

#define PCI_SLOT(devfn) (((devfn) >> 3) & 0x1f)
#define PCI_FUNC(devfn) ((devfn) & 0x07)

class Id {
public:
  Id(const std::string &str);

  Id(int vid = 0, int did = 0, int cc = 0)
      : vendor(vid), device(did), class_code(cc) {}

  bool operator==(const Id &i);

  unsigned int vendor;
  unsigned int device;
  unsigned int class_code;
};

class Slot {
public:
  Slot(const std::string &str);

  Slot(int dom = 0, int b = 0, int dev = 0, int fcn = 0)
      : domain(dom), bus(b), device(dev), function(fcn) {}

  bool operator==(const Slot &s);

  unsigned int domain;
  unsigned int bus;
  unsigned int device;
  unsigned int function;
};

struct Region {
  int num;
  uintptr_t start;
  uintptr_t end;
  unsigned long long flags;
};

class PciDevice {
public:
  PciDevice(Id i, Slot s) : id(i), slot(s), log(Log::get("kernel:pci")) {}

  PciDevice(Id i) : id(i), log(Log::get("kernel:pci")) {}

  PciDevice(Slot s) : slot(s), log(Log::get("kernel:pci")) {}

  bool operator==(const PciDevice &other);

  // Get currently loaded driver for device
  std::string getDriver() const;

  // Bind a new LKM to the PCI device
  bool attachDriver(const std::string &driver) const;

  // Return the IOMMU group of this PCI device or -1 if the device is not in a group
  int getIommuGroup() const;

  std::list<Region> getRegions() const;

  // Write 32-bit BAR value from to the PCI configuration space
  void writeBar(uint32_t addr, unsigned bar = 0);

  // If BAR values in config space and in the kernel do not match, rewrite
  // the BAR value of the kernel to PCIe config space
  void rewriteBar(unsigned bar = 0);

  // Read 32-bit BAR value from the PCI configuration space
  uint32_t readBar(unsigned bar = 0) const;

  // Read 32-bit BAR value from the devices resource file.
  // This is what the kernel thinks the BAR should be.
  uint32_t readHostBar(unsigned bar = 0) const;

  Id id;
  Slot slot;

private:
  villas::Logger log;

protected:
  std::fstream
  openSysFs(const std::string &subPath,
            std::ios_base::openmode mode = std::ios_base::in |
                                           std::ios_base::out) const;
};

class PciDeviceList : public std::list<std::shared_ptr<PciDevice>> {
private:
  // Initialize Linux PCI handle.
  //
  // This search for all available PCI devices under /sys/bus/pci
  PciDeviceList();
  PciDeviceList &operator=(const PciDeviceList &);
  static PciDeviceList *instance;

public:
  static PciDeviceList *getInstance();

  PciDeviceList::value_type lookupDevice(const Slot &s);

  PciDeviceList::value_type lookupDevice(const Id &i);

  PciDeviceList::value_type lookupDevice(const PciDevice &f);
};

} // namespace devices
} // namespace kernel
} // namespace villas
