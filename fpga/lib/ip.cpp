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
#include "dependency_graph.hpp"
#include "utils.hpp"

#include "fpga/ip.hpp"
#include "fpga/card.hpp"

#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <utility>

#include "log.hpp"

using DependencyGraph = villas::utils::DependencyGraph<std::string>;

static
std::list<std::string>
dependencyTokens = {"irqs"};

static
bool
buildDependencyGraph(DependencyGraph& dependencyGraph, json_t* json_ips, std::string name)
{
	const bool nodeExists = dependencyGraph.addNode(name);

	// HACK: just get the right logger
	auto logger = loggerGetOrCreate("IpCoreFactory");

	// do not add IP multiple times
	// this happens if more than 1 IP depends on a certain other IP
	if(nodeExists) {
		return true;
	}

	json_t* json_ip = json_object_get(json_ips, name.c_str());
	if(json_ip == nullptr) {
		logger->error("IP {} not found in config", name);
		return false;
	}

	for(auto& dependencyToken : dependencyTokens) {
		json_t* json_dependency = json_object_get(json_ip, dependencyToken.c_str());
		if(json_dependency == nullptr) {
			logger->debug("Property {} of {} is not present",
			              dependencyToken, TXT_BOLD(name));
			continue;
		}

		const char* irq_name;
		json_t* json_irq;
		json_object_foreach(json_dependency, irq_name, json_irq) {
			const char* value = json_string_value(json_irq);
			if(value == nullptr) {
				logger->warn("Property {} of {} is invalid",
				             dependencyToken, TXT_BOLD(name));
				continue;
			}

			auto mapping = villas::utils::tokenize(value, ":");


			if(mapping.size() != 2) {
				logger->error("Invalid {} mapping of {}",
				              dependencyToken, TXT_BOLD(name));

				dependencyGraph.removeNode(name);
				return false;
			}

			const std::string& dependencyName = mapping[0];

			if(name == dependencyName) {
				logger->error("IP {} cannot depend on itself", TXT_BOLD(name));

				dependencyGraph.removeNode(name);
				return false;
			}

			// already add dependency, if adding it fails, removing the dependency
			// will also remove the current one
			dependencyGraph.addDependency(name, dependencyName);

			if(not buildDependencyGraph(dependencyGraph, json_ips, dependencyName)) {
				logger->error("Dependency {} of {} not satisfied",
				              dependencyName, TXT_BOLD(name));

				dependencyGraph.removeNode(dependencyName);
				return false;
			}
		}
	}

	return true;
}


namespace villas {
namespace fpga {
namespace ip {

void IpCore::dump() {
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
	DependencyGraph dependencyGraph;
	IpCoreList initializedIps;
	auto loggerStatic = getStaticLogger();


	loggerStatic->debug("Parsing IP dependency graph:");
	void* iter = json_object_iter(json_ips);
	while(iter != nullptr) {
		buildDependencyGraph(dependencyGraph, json_ips, json_object_iter_key(iter));
		iter = json_object_iter_next(json_ips, iter);
	}


	loggerStatic->debug("IP initialization order:");
	for(auto& ipName : dependencyGraph.getEvaluationOrder()) {
		loggerStatic->debug("  {}", TXT_BOLD(ipName));
	}


	for(auto& ipName : dependencyGraph.getEvaluationOrder()) {
		loggerStatic->info("Initializing {}", TXT_BOLD(ipName));

		json_t* json_ip = json_object_get(json_ips, ipName.c_str());

		// extract VLNV from JSON
		const char* vlnv;
		if(json_unpack(json_ip, "{ s: s }", "vlnv", &vlnv) != 0) {
			loggerStatic->warn("IP {} has no entry 'vlnv'", ipName);
			continue;
		}

		IpIdentifier id(Vlnv(vlnv), ipName);

		// find the appropriate factory that can create the specified VLNV
		// Note:
		// This is the magic part! Factories automatically register as a
		// plugin as soon as they are instantiated. If there are multiple
		// candidates, the first suitable factory will be used.
		IpCoreFactory* ipCoreFactory = lookup(id.vlnv);

		if(ipCoreFactory == nullptr) {
			loggerStatic->warn("No plugin found to handle {}", vlnv);
			continue;
		} else {
			loggerStatic->debug("Using {} for IP {}",
			                    ipCoreFactory->getName(), vlnv);
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

		// TODO: currently fails, fix and remove comment
		if(not ip->init()) {
			logger->error("Cannot start IP {}", ip->id.name);
			continue;
		}

		if(not ip->check()) {
			logger->error("Checking of IP {} failed", ip->id.name);
			continue;
		}

		initializedIps.push_back(std::move(ip));
	}


	return initializedIps;
}

} // namespace ip
} // namespace fpga
} // namespace villas
