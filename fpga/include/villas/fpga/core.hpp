/* Interlectual Property component.
 *
 * This class represents a module within the FPGA.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <fmt/ostream.h>
#include <jansson.h>
#include <list>
#include <map>
#include <memory>
#include <villas/colors.hpp>
#include <villas/config.hpp>
#include <villas/fpga/vlnv.hpp>
#include <villas/log.hpp>
#include <villas/memory.hpp>
#include <villas/plugin.hpp>

namespace villas {
namespace fpga {

// Forward declarations
class Card;

namespace ip {

// Forward declarations
class Core;
class CoreFactory;
class InterruptController;

class IpIdentifier {
public:
  IpIdentifier(const Vlnv &vlnv = Vlnv::getWildcard(),
               const std::string &name = "")
      : vlnv(vlnv), name(name) {}

  IpIdentifier(const std::string &vlnvString, const std::string &name = "")
      : vlnv(vlnvString), name(name) {}

  const std::string &getName() const { return name; }

  const Vlnv &getVlnv() const { return vlnv; }

  friend std::ostream &operator<<(std::ostream &stream,
                                  const IpIdentifier &id) {
    return stream << id.name << " vlnv=" << id.vlnv;
  }

  bool operator==(const IpIdentifier &otherId) const {
    const bool vlnvWildcard = otherId.getVlnv() == Vlnv::getWildcard();
    const bool nameWildcard =
        this->getName().empty() or otherId.getName().empty();

    const bool vlnvMatch = vlnvWildcard or this->getVlnv() == otherId.getVlnv();
    const bool nameMatch = nameWildcard or this->getName() == otherId.getName();

    return vlnvMatch and nameMatch;
  }

  bool operator!=(const IpIdentifier &otherId) const {
    return !(*this == otherId);
  }

private:
  Vlnv vlnv;
  std::string name;
};

class Core {
  friend CoreFactory;

public:
  Core() : card(nullptr) {}

  virtual ~Core() = default;

public:
  // Generic management interface for IPs

  // Runtime setup of IP, should access and initialize hardware
  virtual bool init() { return true; }

  // Runtime check of IP, should verify basic functionality
  virtual bool check() { return true; }

  // Generic disabling of IP, meaning may depend on IP
  virtual bool stop() { return true; }

  // Reset the IP, it should behave like freshly initialized afterwards
  virtual bool reset() { return true; }

  // Print some debug information about the IP
  virtual void dump();

protected:
  // Key-type for accessing maps addressTranslations and slaveAddressSpaces
  using MemoryBlockName = std::string;

  // Each IP can declare via this function which memory blocks it requires
  virtual std::list<MemoryBlockName> getMemoryBlocks() const { return {}; }

public:
  size_t getBaseaddr() const { return baseaddr; }
  const std::string &getInstanceName() const { return id.getName(); }

  // Operators

  bool operator==(const Vlnv &otherVlnv) const {
    return id.getVlnv() == otherVlnv;
  }

  bool operator!=(const Vlnv &otherVlnv) const {
    return id.getVlnv() != otherVlnv;
  }

  bool operator==(const IpIdentifier &otherId) const {
    return this->id == otherId;
  }

  bool operator!=(const IpIdentifier &otherId) const {
    return this->id != otherId;
  }

  bool operator==(const std::string &otherName) const {
    return getInstanceName() == otherName;
  }

  bool operator!=(const std::string &otherName) const {
    return getInstanceName() != otherName;
  }

  bool operator==(const Core &otherIp) const { return this->id == otherIp.id; }

  bool operator!=(const Core &otherIp) const { return this->id != otherIp.id; }

  friend std::ostream &operator<<(std::ostream &stream, const Core &ip) {
    return stream << ip.id;
  }

protected:
  uintptr_t getBaseAddr(const MemoryBlockName &block) const {
    return getLocalAddr(block, 0);
  }

  uintptr_t getLocalAddr(const MemoryBlockName &block, uintptr_t address) const;

  MemoryManager::AddressSpaceId
  getAddressSpaceId(const MemoryBlockName &block) const {
    return slaveAddressSpaces.at(block);
  }

  InterruptController *
  getInterruptController(const std::string &interruptName) const;

  MemoryManager::AddressSpaceId
  getMasterAddrSpaceByInterface(const std::string &masterInterfaceName) const {
    return busMasterInterfaces.at(masterInterfaceName);
  }

  template <typename T>
  T readMemory(const std::string &block, uintptr_t address) const {
    return *(reinterpret_cast<T *>(getLocalAddr(block, address)));
  }

  template <typename T>
  void writeMemory(const std::string &block, uintptr_t address, T value) {
    T *ptr = reinterpret_cast<T *>(getLocalAddr(block, address));
    *ptr = value;
  }

protected:
  struct IrqPort {
    int num;
    InterruptController *irqController;
    std::string description;
  };

  // Specialized logger instance with the IPs name set as category
  Logger logger;

  // FPGA card this IP is instantiated on (populated by FpgaIpFactory)
  Card *card;

  // Identifier of this IP with its instance name and VLNV
  IpIdentifier id;

  // All interrupts of this IP with their associated interrupt controller
  std::map<std::string, IrqPort> irqs;

  // Cached translations from the process address space to each memory block
  std::map<MemoryBlockName, MemoryTranslation> addressTranslations;

  // Lookup for IP's slave address spaces (= memory blocks)
  std::map<MemoryBlockName, MemoryManager::AddressSpaceId> slaveAddressSpaces;

  // AXI bus master interfaces to access memory somewhere
  std::map<std::string, MemoryManager::AddressSpaceId> busMasterInterfaces;

  size_t baseaddr = 0;
};

class CoreFactory : public plugin::Plugin {
public:
  using plugin::Plugin::Plugin;

  static std::list<IpIdentifier> parseIpIdentifier(json_t *json_ips);
  static std::list<IpIdentifier>
  filterIps(std::list<IpIdentifier> allIps,
            std::list<std::string> ignored_ip_names);
  static std::list<IpIdentifier> reorderIps(std::list<IpIdentifier> allIps);
  static std::list<std::shared_ptr<Core>>
  configureIps(std::list<IpIdentifier> orderedIps, json_t *json_ips,
               Card *card);
  static void initIps(std::list<std::shared_ptr<Core>> orderedIps, Card *card);

  // Returns a running and checked FPGA IP
  static std::list<std::shared_ptr<Core>> make(Card *card, json_t *json_ips);

  virtual std::string getType() const { return "core"; }

protected:
  enum PollingMode {
    POLL,
    IRQ,
  };

  Logger getLogger() { return villas::Log::get(getName()); }

  // Configure IP instance from JSON config
  virtual void parse(Core &, json_t *) {}

  static Logger getStaticLogger() { return villas::Log::get("core:factory"); }

private:
  virtual void configurePollingMode(Core &, PollingMode) {}

  virtual Vlnv getCompatibleVlnv() const = 0;

  // Create a concrete IP instance
  virtual Core *make() const = 0;

  static CoreFactory *lookup(const Vlnv &vlnv);
};

template <typename T, const char *name, const char *desc, const char *vlnv>
class CorePlugin : public CoreFactory {

public:
  virtual std::string getName() const { return name; }

  virtual std::string getDescription() const { return desc; }

private:
  virtual Vlnv getCompatibleVlnv() const { return Vlnv(vlnv); }

  // Create a concrete IP instance
  Core *make() const { return new T; };
};

} // namespace ip
} // namespace fpga
} // namespace villas

#ifndef FMT_LEGACY_OSTREAM_FORMATTER
template <>
class fmt::formatter<villas::fpga::ip::IpIdentifier>
    : public fmt::ostream_formatter {};
template <>
class fmt::formatter<villas::fpga::ip::Core> : public fmt::ostream_formatter {};
#endif
