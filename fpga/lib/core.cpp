/* FPGA IP component.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <memory>
#include <string>
#include <utility>

#include <villas/exceptions.hpp>
#include <villas/log.hpp>
#include <villas/memory.hpp>
#include <villas/utils.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/utils.hpp>
#include <villas/fpga/vlnv.hpp>

#include <villas/fpga/core.hpp>
#include <villas/fpga/ips/intc.hpp>
#include <villas/fpga/ips/pcie.hpp>
#include <villas/fpga/ips/switch.hpp>

using namespace villas::fpga;
using namespace villas::fpga::ip;

// Special IPs that have to be initialized first. Will be initialized in the
// same order as they appear in this list, i.e. first here will be initialized
// first.
static std::list<Vlnv> vlnvInitializationOrder = {
    Vlnv("xilinx.com:ip:axi_pcie:"),
    Vlnv("xilinx.com:ip:xdma:"),
    Vlnv("xilinx.com:module_ref:axi_pcie_intc:"),
    Vlnv("xilinx.com:ip:axis_switch:"),
    Vlnv("xilinx.com:ip:axi_iic:"),
};

std::list<IpIdentifier> CoreFactory::parseIpIdentifier(json_t *json_ips) {
  // Parse all IP instance names and their VLNV into list `allIps`
  std::list<IpIdentifier> allIps;

  const char *ipName;
  json_t *json_ip;
  json_object_foreach(json_ips, ipName, json_ip) {
    const char *vlnv;

    json_error_t err;
    int ret = json_unpack_ex(json_ip, &err, 0, "{ s: s }", "vlnv", &vlnv);
    if (ret != 0)
      throw ConfigError(json_ip, err, "", "IP {} has no VLNV", ipName);

    allIps.push_back({vlnv, ipName});
  }
  return allIps;
}

std::list<IpIdentifier>
CoreFactory::reorderIps(std::list<IpIdentifier> allIps) {
  // Pick out IPs to be initialized first.
  std::list<IpIdentifier> orderedIps;

  // Reverse walktrough, because we push to the
  // front of the output list, so that the first element will also be the
  // first to be initialized.
  for (auto viIt = vlnvInitializationOrder.rbegin();
       viIt != vlnvInitializationOrder.rend(); ++viIt) {
    // Iterate over IPs, if VLNV matches, push to front and remove from list
    for (auto it = allIps.begin(); it != allIps.end(); ++it) {
      if (*viIt == it->getVlnv()) {
        orderedIps.push_front(*it);
        it = allIps.erase(it);
      }
    }
  }

  // Insert all other IPs at the end
  orderedIps.splice(orderedIps.end(), allIps);

  auto loggerStatic = CoreFactory::getStaticLogger();
  loggerStatic->debug("IP initialization order:");
  for (auto &id : orderedIps) {
    loggerStatic->debug("  " CLR_BLD("{}"), id.getName());
  }

  return orderedIps;
}

std::list<std::shared_ptr<Core>>
CoreFactory::configureIps(std::list<IpIdentifier> orderedIps, json_t *json_ips,
                          Card *card) {
  std::list<std::shared_ptr<Core>> configuredIps;
  auto loggerStatic = CoreFactory::getStaticLogger();
  // Configure all IPs
  for (auto &id : orderedIps) {
    loggerStatic->info("Configuring {}", id);

    // Find the appropriate factory that can create the specified VLNV
    // Note:
    // This is the magic part! Factories automatically register as a
    // plugin as soon as they are instantiated. If there are multiple
    // candidates, the first suitable factory will be used.
    auto *f = lookup(id.getVlnv());
    if (f == nullptr) {
      loggerStatic->warn("No plugin found to handle {}", id.getVlnv());
      continue;
    } else
      loggerStatic->debug("Using {} for IP {}", f->getName(), id.getVlnv());

    auto logger = f->getLogger();

    // Create new IP instance. Since this function is virtual, it will
    // construct the right, specialized type without knowing it here
    // because we have already picked the right factory.
    // If something goes wrong with initialization, the shared_ptr will
    // take care to desctruct the Core again as it is not pushed to
    // the list and will run out of scope.
    auto ip = std::unique_ptr<Core>(f->make());

    if (ip == nullptr) {
      logger->warn("Cannot create an instance of {}", f->getName());
      continue;
    }

    // Setup generic IP type properties
    ip->card = card;
    ip->id = id;
    ip->logger = villas::logging.get(id.getName());

    json_t *json_ip = json_object_get(json_ips, id.getName().c_str());

    // parse ip baseadress
    json_t *json_parameters = json_object_get(json_ip, "parameters");
    if (json_is_object(json_parameters)) {
      json_int_t c_baseaddr = 0;
      int baseaddress_found =
          json_unpack(json_parameters, "{ s?: I }", "c_baseaddr", &c_baseaddr);
      if (baseaddress_found == 0)
        ip->baseaddr = c_baseaddr;
    }

    json_t *json_irqs = json_object_get(json_ip, "irqs");
    if (json_is_object(json_irqs)) {
      logger->debug("Parse IRQs of {}", *ip);

      const char *irqName;
      json_t *json_irq;
      json_object_foreach(json_irqs, irqName, json_irq) {
        const char *irqEntry = json_string_value(json_irq);

        auto tokens = utils::tokenize(irqEntry, ":");
        if (tokens.size() != 2) {
          logger->warn("Cannot parse IRQ '{}' of " CLR_BLD("{}"), irqEntry,
                       id.getName());
          continue;
        }

        const std::string &irqControllerName = tokens[0];
        InterruptController *intc = nullptr;

        for (auto &configuredIp : configuredIps) {
          if (*configuredIp == irqControllerName) {
            intc = dynamic_cast<InterruptController *>(configuredIp.get());
            break;
          }
        }

        if (intc == nullptr) {
          logger->error("Interrupt Controller {} for IRQ {} not found",
                        irqControllerName, irqName);
          continue;
        }

        int num;
        try {
          num = std::stoi(tokens[1]);
        } catch (const std::invalid_argument &) {
          logger->warn("IRQ number is not an integer: '{}'", irqEntry);
          continue;
        }
        logger->debug("IRQ: {} -> {}:{}", irqName, irqControllerName, num);
        ip->irqs[irqName] = {num, intc, ""};
      }
    }

    json_t *json_memory_view = json_object_get(json_ip, "memory-view");
    if (json_is_object(json_memory_view)) {
      logger->debug("Parse memory view of {}", *ip);

      // Now find all slave address spaces this master can access
      const char *bus_name;
      json_t *json_bus;
      json_object_foreach(json_memory_view, bus_name, json_bus) {

        // This IP has a memory view => it is a bus master somewhere

        // Assemble name for master address space
        const std::string myAddrSpaceName =
            MemoryManager::getMasterAddrSpaceName(ip->getInstanceName(),
                                                  bus_name);
        // Create a master address space
        const MemoryManager::AddressSpaceId myAddrSpaceId =
            MemoryManager::get().getOrCreateAddressSpace(myAddrSpaceName);

        ip->busMasterInterfaces[bus_name] = myAddrSpaceId;

        const char *instance_name;
        json_t *json_instance;
        json_object_foreach(json_bus, instance_name, json_instance) {

          const char *block_name;
          json_t *json_block;
          json_object_foreach(json_instance, block_name, json_block) {

            json_int_t base, high, size;
            json_error_t err;
            int ret = json_unpack_ex(json_block, &err, 0,
                                     "{ s: I, s: I, s: I }", "baseaddr", &base,
                                     "highaddr", &high, "size", &size);
            if (ret != 0)
              throw ConfigError(
                  json_block, err, "", "Cannot parse address block {}/{}/{}/{}",
                  ip->getInstanceName(), bus_name, instance_name, block_name);

            // Get or create the slave address space
            const std::string slaveAddrSpace =
                MemoryManager::getSlaveAddrSpaceName(instance_name, block_name);

            const MemoryManager::AddressSpaceId slaveAddrSpaceId =
                MemoryManager::get().getOrCreateAddressSpace(slaveAddrSpace);

            // Create a new mapping to the slave address space
            MemoryManager::get().createMapping(
                static_cast<uintptr_t>(base), 0, static_cast<uintptr_t>(size),
                bus_name, myAddrSpaceId, slaveAddrSpaceId);
          }
        }
      }
    }

    // IP-specific setup via JSON config
    f->parse(*ip, json_ip);

    // Set polling mode
    f->configurePollingMode(
        *ip, (card->polling ? PollingMode::POLL : PollingMode::IRQ));

    // IP has been configured now
    configuredIps.push_back(std::move(ip));
  }

  return configuredIps;
}

void CoreFactory::initIps(std::list<std::shared_ptr<Core>> configuredIps,
                          Card *card) {
  auto loggerStatic = CoreFactory::getStaticLogger();
  // Start and check IPs now
  for (auto &ip : configuredIps) {
    loggerStatic->info("Initializing {}", *ip);

    // Translate all memory blocks that the IP needs to be accessible from
    // the process and cache in the instance, so this has not to be done at
    // runtime.
    for (auto &memoryBlock : ip->getMemoryBlocks()) {
      // Construct the global name of this address block
      const auto addrSpaceName = MemoryManager::getSlaveAddrSpaceName(
          ip->getInstanceName(), memoryBlock);

      // Retrieve its address space identifier
      const auto addrSpaceId =
          MemoryManager::get().findAddressSpace(addrSpaceName);

      // ... and save it in IP
      ip->slaveAddressSpaces.emplace(memoryBlock, addrSpaceId);

      // Get the translation to the address space
      const auto &translation =
          MemoryManager::get().getTranslationFromProcess(addrSpaceId);

      // Cache it in the IP instance only with local name
      ip->addressTranslations.emplace(memoryBlock, translation);
    }

    if (not ip->init()) {
      throw RuntimeError("Failed to initialize IP {}", *ip);
    }

    if (not ip->check()) {
      throw RuntimeError("Failed to check IP {}", *ip);
    }

    // Will only be reached if the IP successfully was initialized
    card->ips.push_back(std::move(ip));
  }

  loggerStatic->debug("Initialized IPs:");
  for (auto &ip : card->ips) {
    loggerStatic->debug("  {}", *ip);
  }
}

std::list<std::shared_ptr<Core>> CoreFactory::make(Card *card,
                                                   json_t *json_ips) {
  // We only have this logger until we know the factory to build an IP with
  auto loggerStatic = getStaticLogger();

  if (!card->ips.empty()) {
    loggerStatic->error("IP list of card {} already contains IPs.", card->name);
    throw RuntimeError("IP list of card {} already contains IPs.", card->name);
  }

  std::list<IpIdentifier> allIps =
      parseIpIdentifier(json_ips); // All IPs available in config

  std::list<IpIdentifier> orderedIps =
      reorderIps(allIps); // IPs ordered in initialization order

  std::list<std::shared_ptr<Core>> configuredIps =
      configureIps(orderedIps, json_ips, card); // Successfully configured IPs

  initIps(configuredIps, card);

  return card->ips;
}

void Core::dump() {
  logger->info("IP: {}", *this);
  for (auto &[num, irq] : irqs) {
    logger->info("  IRQ {}: {}:{}", num, irq.irqController->getInstanceName(),
                 irq.num);
  }

  for (auto &[block, translation] : addressTranslations) {
    logger->info("  Memory {}: {}", block, translation);
  }
}

CoreFactory *CoreFactory::lookup(const Vlnv &vlnv) {
  for (auto &ip : plugin::registry->lookup<CoreFactory>()) {
    if (ip->getCompatibleVlnv() == vlnv)
      return ip;
  }

  return nullptr;
}

uintptr_t Core::getLocalAddr(const MemoryBlockName &block,
                             uintptr_t address) const {
  // Throws exception if block not present
  auto &translation = addressTranslations.at(block);

  return translation.getLocalAddr(address);
}

InterruptController *
Core::getInterruptController(const std::string &interruptName) const {
  try {
    const IrqPort irq = irqs.at(interruptName);
    return irq.irqController;
  } catch (const std::out_of_range &) {
    return nullptr;
  }
}
