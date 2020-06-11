/** FPGA IP component.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASfpga
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <string>
#include <memory>
#include <utility>

#include <villas/log.hpp>
#include <villas/memory.hpp>
#include <villas/utils.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/vlnv.hpp>

#include <villas/fpga/ip.hpp>
#include <villas/fpga/ips/pcie.hpp>
#include <villas/fpga/ips/intc.hpp>
#include <villas/fpga/ips/switch.hpp>


namespace villas {
namespace fpga {
namespace ip {

// Special IPs that have to be initialized first. Will be initialized in the
// same order as they appear in this list, i.e. first here will be initialized
// first.
static std::list<Vlnv>
vlnvInitializationOrder = {
    Vlnv(AxiPciExpressBridgeFactory::getCompatibleVlnvString()),
    Vlnv(InterruptControllerFactory::getCompatibleVlnvString()),
    Vlnv(AxiStreamSwitchFactory::getCompatibleVlnvString()),
};


IpCoreList
IpCoreFactory::make(PCIeCard* card, json_t *json_ips)
{
	// We only have this logger until we know the factory to build an IP with
	auto loggerStatic = getStaticLogger();

	std::list<IpIdentifier> allIps;		// all IPs available in config
	std::list<IpIdentifier> orderedIps;	// IPs ordered in initialization order

	IpCoreList configuredIps;	// Successfully configured IPs
	IpCoreList initializedIps;	// Initialized, i.e. ready-to-use IPs


	// parse all IP instance names and their VLNV into list `allIps`
	const char* ipName;
	json_t* json_ip;
	json_object_foreach(json_ips, ipName, json_ip) {
		const char* vlnv;
		if(json_unpack(json_ip, "{ s: s }", "vlnv", &vlnv) != 0) {
			loggerStatic->warn("IP {} has no VLNV", ipName);
			continue;
		}

		allIps.push_back({vlnv, ipName});
	}

	// Pick out IPs to be initialized first.
	//
	// Reverse order of the initialization order list, because we push to the
	// front of the output list, so that the first element will also be the
	// first to be initialized.
	vlnvInitializationOrder.reverse();

	for(auto& vlnvInitFirst : vlnvInitializationOrder) {
		// iterate over IPs, if VLNV matches, push to front and remove from list
		for(auto it = allIps.begin(); it != allIps.end(); ++it) {
			if(vlnvInitFirst == it->getVlnv()) {
				orderedIps.push_front(*it);
				it = allIps.erase(it);
			}
		}
	}

	// insert all other IPs at the end
	orderedIps.splice(orderedIps.end(), allIps);

	loggerStatic->debug("IP initialization order:");
	for(auto& id : orderedIps) {
		loggerStatic->debug("  " CLR_BLD("{}"), id.getName());
	}

	// configure all IPs
	for(auto& id : orderedIps) {
		loggerStatic->info("Configuring {}", id);

		// find the appropriate factory that can create the specified VLNV
		// Note:
		// This is the magic part! Factories automatically register as a
		// plugin as soon as they are instantiated. If there are multiple
		// candidates, the first suitable factory will be used.
		IpCoreFactory* ipCoreFactory = lookup(id.getVlnv());

		if(ipCoreFactory == nullptr) {
			loggerStatic->warn("No plugin found to handle {}", id.getVlnv());
			continue;
		} else {
			loggerStatic->debug("Using {} for IP {}",
			                    ipCoreFactory->getName(), id.getVlnv());
		}

		auto logger = ipCoreFactory->getLogger();

		// Create new IP instance. Since this function is virtual, it will
		// construct the right, specialized type without knowing it here
		// because we have already picked the right factory.
		// If something goes wrong with initialization, the shared_ptr will
		// take care to desctruct the IpCore again as it is not pushed to
		// the list and will run out of scope.
		auto ip = std::unique_ptr<IpCore>(ipCoreFactory->create());

		if(ip == nullptr) {
			logger->warn("Cannot create an instance of {}",
			             ipCoreFactory->getName());
			continue;
		}

		// setup generic IP type properties
		ip->card = card;
		ip->id = id;
		ip->logger = villas::logging.get(id.getName());

		json_t* json_ip = json_object_get(json_ips, id.getName().c_str());

		json_t* json_irqs = json_object_get(json_ip, "irqs");
		if(json_is_object(json_irqs)) {
			logger->debug("Parse IRQs of {}", *ip);

			const char* irqName;
			json_t* json_irq;
			json_object_foreach(json_irqs, irqName, json_irq) {
				const char* irqEntry = json_string_value(json_irq);


				auto tokens = utils::tokenize(irqEntry, ":");
				if(tokens.size() != 2) {
					logger->warn("Cannot parse IRQ '{}' of " CLR_BLD("{}"),
					             irqEntry, id.getName());
					continue;
				}

				const std::string& irqControllerName = tokens[0];
				InterruptController* intc = nullptr;

				for(auto& configuredIp : configuredIps) {
					if(*configuredIp == irqControllerName) {
						intc = dynamic_cast<InterruptController*>(configuredIp.get());
						break;
					}
				}

				if(intc == nullptr) {
					logger->error("Interrupt Controller {} for IRQ {} not found",
					              irqControllerName, irqName);
					continue;
				}

				int num;
				try {
					num = std::stoi(tokens[1]);
				} catch(const std::invalid_argument&) {
					logger->warn("IRQ number is not an integer: '{}'", irqEntry);
					continue;
				}
				logger->debug("IRQ: {} -> {}:{}", irqName, irqControllerName, num);
				ip->irqs[irqName] = {num, intc, ""};
			}
		}


		json_t* json_memory_view = json_object_get(json_ip, "memory-view");
		if(json_is_object(json_memory_view)) {
			logger->debug("Parse memory view of {}", *ip);


			// now find all slave address spaces this master can access
			const char* bus_name;
			json_t* json_bus;
			json_object_foreach(json_memory_view, bus_name, json_bus) {

				// this IP has a memory view => it is a bus master somewhere

				// assemble name for master address space
				const std::string myAddrSpaceName =
				        MemoryManager::getMasterAddrSpaceName(ip->getInstanceName(),
				                                              bus_name);
				// create a master address space
				const MemoryManager::AddressSpaceId myAddrSpaceId =
				        MemoryManager::get().getOrCreateAddressSpace(myAddrSpaceName);

				ip->busMasterInterfaces[bus_name] = myAddrSpaceId;


				const char* instance_name;
				json_t* json_instance;
				json_object_foreach(json_bus, instance_name, json_instance) {

					const char* block_name;
					json_t* json_block;
					json_object_foreach(json_instance, block_name, json_block) {

						json_int_t base, high, size;
						int ret = json_unpack(json_block, "{ s: I, s: I, s: I }",
						                      "baseaddr", &base,
						                      "highaddr", &high,
						                      "size", &size);
						if(ret != 0) {
							logger->error("Cannot parse address block {}/{}/{}/{}",
							              ip->getInstanceName(),
							              bus_name, instance_name, block_name);
							continue;

						}

						// get or create the slave address space
						const std::string slaveAddrSpace =
						        MemoryManager::getSlaveAddrSpaceName(instance_name, block_name);

						const MemoryManager::AddressSpaceId slaveAddrSpaceId =
						        MemoryManager::get().getOrCreateAddressSpace(slaveAddrSpace);

						// create a new mapping to the slave address space
						MemoryManager::get().createMapping(static_cast<uintptr_t>(base),
						                                   0,
						                                   static_cast<uintptr_t>(size),
						                                   bus_name,
						                                   myAddrSpaceId,
						                                   slaveAddrSpaceId);
					}
				}
			}
		}

		// IP-specific setup via JSON config
		if(not ipCoreFactory->configureJson(*ip, json_ip)) {
			logger->warn("Cannot configure IP from JSON");
			continue;
		}

		// IP has been configured now
		configuredIps.push_back(std::move(ip));
	}

	// Start and check IPs now
	for(auto& ip : configuredIps) {

		// Translate all memory blocks that the IP needs to be accessible from
		// the process and cache in the instance, so this has not to be done at
		// runtime.
		for(auto& memoryBlock : ip->getMemoryBlocks()) {
			// construct the global name of this address block
			const auto addrSpaceName =
			        MemoryManager::getSlaveAddrSpaceName(ip->getInstanceName(),
			                                             memoryBlock);

			// retrieve its address space identifier
			const auto addrSpaceId =
			        MemoryManager::get().findAddressSpace(addrSpaceName);

			// ... and save it in IP
			ip->slaveAddressSpaces.emplace(memoryBlock, addrSpaceId);

			// get the translation to the address space
			const auto& translation =
			        MemoryManager::get().getTranslationFromProcess(addrSpaceId);

			// cache it in the IP instance only with local name
			ip->addressTranslations.emplace(memoryBlock, translation);
		}

		loggerStatic->info("Initializing {}", *ip);

		if(not ip->init()) {
			loggerStatic->error("Cannot start IP {}", *ip);
			continue;
		}

		if(not ip->check()) {
			loggerStatic->error("Checking failed for IP {}", *ip);
			continue;
		}

		// will only be reached if the IP successfully was initialized
		initializedIps.push_back(std::move(ip));
	}


	loggerStatic->debug("Initialized IPs:");
	for(auto& ip : initializedIps) {
		loggerStatic->debug("  {}", *ip);
	}

	return initializedIps;
}


void
IpCore::dump()
{
	logger->info("IP: {}", *this);
	for(auto& [num, irq] : irqs) {
		logger->info("  IRQ {}: {}:{}",
		             num, irq.irqController->getInstanceName(), irq.num);
	}

	for(auto& [block, translation] : addressTranslations) {
		logger->info("  Memory {}: {}", block, translation);
	}
}


IpCoreFactory*
IpCoreFactory::lookup(const Vlnv &vlnv)
{
	for(auto& ip : plugin::Registry::lookup<IpCoreFactory>()) {
		if(ip->getCompatibleVlnv() == vlnv)
			return ip;
	}

	return nullptr;
}


uintptr_t
IpCore::getLocalAddr(const MemoryBlockName& block, uintptr_t address) const
{
	// throws exception if block not present
	auto& translation = addressTranslations.at(block);

	return translation.getLocalAddr(address);
}


InterruptController*
IpCore::getInterruptController(const std::string& interruptName) const
{
	try {
		const IrqPort irq = irqs.at(interruptName);
		return irq.irqController;
	} catch(const std::out_of_range&) {
		return nullptr;
	}
}


} // namespace ip
} // namespace fpga
} // namespace villas
