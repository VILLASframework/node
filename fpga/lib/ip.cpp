/** FPGA IP component.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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

#include "log_config.h"
#include "log.hpp"
#include "plugin.h"
#include "utils.hpp"

#include "fpga/vlnv.hpp"
#include "fpga/ip.hpp"
#include "fpga/card.hpp"

#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <utility>

#include "log.hpp"

#include "fpga/ips/pcie.hpp"
#include "fpga/ips/intc.hpp"
#include "fpga/ips/switch.hpp"



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

void
IpCore::dump() {
	auto logger = getLogger();

	logger->info("Base address = {:08x}", baseaddr);
	for(auto& [num, irq] : irqs) {
		logger->info("IRQ {}: {}:{}", num, irq.controllerName, irq.num);
	}
}


IpCoreFactory* IpCoreFactory::lookup(const Vlnv &vlnv)
{
	for(auto& ip : Plugin::lookup(Plugin::Type::FpgaIp)) {
		IpCoreFactory* ipCoreFactory = dynamic_cast<IpCoreFactory*>(ip);

		if(ipCoreFactory->getCompatibleVlnv() == vlnv)
			return ipCoreFactory;
	}

	return nullptr;
}

uintptr_t
IpCore::getAddrMapped(uintptr_t address) const
{
	assert(card != nullptr);
	return reinterpret_cast<uintptr_t>(card->map) + address;
}


IpCoreList
IpCoreFactory::make(PCIeCard* card, json_t *json_ips)
{
	IpCoreList configuredIps;
	auto loggerStatic = getStaticLogger();

	std::list<IpIdentifier> allIps;		// all IPs available
	std::list<IpIdentifier> orderedIps;	// IPs ordered in initialization order

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
		loggerStatic->debug("  {}", TXT_BOLD(id.getName()));
	}

	for(auto& id : orderedIps) {
		loggerStatic->info("Initializing {}", id);

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

		// extract base address if it has one
		if(json_unpack(json_ip, "{ s?: i }", "baseaddr", &ip->baseaddr) != 0) {
			logger->warn("Problem while parsing base address of IP {}",
			             TXT_BOLD(ipName));
			continue;
		}

		json_t* json_irqs = json_object_get(json_ip, "irqs");
		if(json_is_object(json_irqs)) {
			const char* irq_name;
			json_t* json_irq;
			json_object_foreach(json_irqs, irq_name, json_irq) {
				const char* irq = json_string_value(json_irq);


				auto tokens = utils::tokenize(irq, ":");
				if(tokens.size() != 2) {
					logger->warn("Cannot parse IRQ '{}' of {}",
					             irq, TXT_BOLD(ipName));
					continue;
				}

				int num;
				try {
					num = std::stoi(tokens[1]);
				} catch(const std::invalid_argument&) {
					logger->warn("IRQ number is not an integer: '{}'", irq);
					continue;
				}
				logger->debug("IRQ: {} -> {}:{}", irq_name, tokens[0], num);
				ip->irqs[irq_name] = {num, tokens[0], ""};
			}
		} else {
			logger->debug("IP has no interrupts");
		}


		bool dependenciesOk = true;
		for(auto& [depName, depVlnv] : ipCoreFactory->getDependencies()) {
			// lookup dependency IP core in list of already initialized IPs
			auto iter = std::find_if(initializedIps.begin(),
			                         initializedIps.end(),
			                         [&](const std::unique_ptr<IpCore>& ip) {
				                         return *ip == depVlnv;
			                         });

			if(iter == initializedIps.end()) {
				logger->error("Cannot find '{}' dependency {} of {}",
				              depName, depVlnv, TXT_BOLD(ipName));
				dependenciesOk = false;
				break;
			}

			logger->debug("Found dependency IP {}", (*iter)->id);
			ip->dependencies[depName] = (*iter).get();
		}

		if(not dependenciesOk) {
			continue;
		}

		// IP-specific setup via JSON config
		if(not ipCoreFactory->configureJson(*ip, json_ip)) {
			logger->warn("Cannot configure IP from JSON");
			continue;
		}

		configuredIps.push_back(std::move(ip));
	}

	IpCoreList initializedIps;
	for(auto& ip : configuredIps) {

		// Translate all memory blocks that the IP needs to be accessible from
		// the process and cache in the instance, so this has not to be done at
		// runtime.
		for(auto& memoryBlock : ip->getMemoryBlocks()) {
			// construct the global name of this address block
			const auto addrSpaceName =
			        MemoryManager::getSlaveAddrSpaceName(ip->id.getName(), memoryBlock);

			// retrieve its address space identifier
			const auto addrSpace =
			        MemoryManager::get().findAddressSpace(addrSpaceName);

			// get the translation to the address space
			const auto& translation =
			        MemoryManager::get().getTranslationFromProcess(addrSpace);

			// cache it in the IP instance only with local name
			ip->addressTranslations.emplace(memoryBlock, translation);
		}


		if(not ip->init()) {
			loggerStatic->error("Cannot start IP {}", ip->id.getName());
			continue;
		}

		if(not ip->check()) {
			loggerStatic->error("Checking of IP {} failed", ip->id.getName());
			continue;
		}

		initializedIps.push_back(std::move(ip));
	}


	return initializedIps;
}

} // namespace ip
} // namespace fpga
} // namespace villas
