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
dependencyTokens = {"irq", "port", "memory"};

static
bool
buildDependencyGraph(DependencyGraph& dependencyGraph, json_t* json_ips, std::string name)
{
//	cpp_debug << "preparse " << name;

	const bool nodeExists = dependencyGraph.addNode(name);

	// do not add IP multiple times
	// this happens if more than 1 IP depends on a certain other IP
	if(nodeExists) {
		return true;
	}

	json_t* json_ip = json_object_get(json_ips, name.c_str());
	if(json_ip == nullptr) {
		cpp_error << "IP " << name << " not found in config";
		return false;
	}

	for(auto& dependencyToken : dependencyTokens) {
		json_t* json_dependency = json_object_get(json_ip, dependencyToken.c_str());
		if(json_dependency == nullptr) {
			cpp_debug << "Property " << dependencyToken << " of " << TXT_BOLD(name)
			          << " not present";
			continue;
		}

		const char* value = json_string_value(json_dependency);
		if(value == nullptr) {
			cpp_warn << "Property " << dependencyToken << " of " << TXT_BOLD(name)
			         << " is invalid";
			continue;
		}

		auto mapping = villas::utils::tokenize(value, ":");


		if(mapping.size() != 2) {
			cpp_error << "Invalid " << dependencyToken << " mapping"
			          << " of " << TXT_BOLD(name);

			dependencyGraph.removeNode(name);
			return false;
		}

		if(name == mapping[0]) {
			cpp_error << "IP " << TXT_BOLD(name)<< " cannot depend on itself";

			dependencyGraph.removeNode(name);
			return false;
		}

		// already add dependency, if adding it fails, removing the dependency
		// will also remove the current one
		dependencyGraph.addDependency(name, mapping[0]);

		if(not buildDependencyGraph(dependencyGraph, json_ips, mapping[0])) {
			cpp_error << "Dependency " << mapping[0] << " of " << TXT_BOLD(name)
			          << " not satified";

			dependencyGraph.removeNode(mapping[0]);
			return false;
		}
	}

	return true;
}


namespace villas {
namespace fpga {
namespace ip {


void IpCore::dump() {
	cpp_info << id;
	{
		Logger::Indenter indent = cpp_info.indent();
		cpp_info << " Baseaddr: 0x" << std::hex << baseaddr << std::dec;
//		cpp_info << " IRQ: " << irq;
//		cpp_info << " Port: " << port;
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
IpCore::getBaseaddr() const {
	assert(card != nullptr);
	return reinterpret_cast<uintptr_t>(card->map) + this->baseaddr;
}


IpCoreList
IpCoreFactory::make(PCIeCard* card, json_t *json_ips)
{
	DependencyGraph dependencyGraph;
	IpCoreList initializedIps;

	{
		Logger::Indenter indent = cpp_debug.indent();
		cpp_debug << "Parsing IP dependency graph:";

		void* iter = json_object_iter(json_ips);
		while(iter != nullptr) {
			buildDependencyGraph(dependencyGraph, json_ips, json_object_iter_key(iter));
			iter = json_object_iter_next(json_ips, iter);
		}
	}

	{
		Logger::Indenter indent = cpp_debug.indent();
		cpp_debug << "IP initialization order:";

		for(auto& ipName : dependencyGraph.getEvaluationOrder()) {
			cpp_debug << TXT_BOLD(ipName);
		}
	}



	cpp_info << "Initializing IP cores";

	Logger::Indenter indent = cpp_info.indent();
	for(auto& ipName : dependencyGraph.getEvaluationOrder()) {
		cpp_debug << TXT_BOLD(ipName);
		json_t* json_ip = json_object_get(json_ips, ipName.c_str());

		// extract VLNV from JSON
		const char* vlnv;
		if(json_unpack(json_ip, "{ s: s }", "vlnv", &vlnv) != 0) {
			cpp_warn << "IP " << ipName << " has no entry 'vlnv'";
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
			cpp_warn << "No plugin found to handle " << vlnv;
			continue;
		} else {
			cpp_debug << "Using " << ipCoreFactory->getName()
			          << " for IP " << vlnv;
		}

		// Create new IP instance. Since this function is virtual, it will
		// construct the right, specialized type without knowing it here
		// because we have already picked the right factory.
		// If something goes wrong with initialization, the shared_ptr will
		// take care to desctruct the IpCore again as it is not pushed to
		// the list and will run out of scope.
		auto ip = std::unique_ptr<IpCore>(ipCoreFactory->create());

		if(ip == nullptr) {
			cpp_warn << "Cannot create an instance of "
			         << ipCoreFactory->getName();
			continue;
		}

		// setup generic IP type properties
		ip->card = card;
		ip->id = id;

		// extract some optional properties
		int ret = json_unpack(json_ip, "{ s?: i, s?: i }",
		                               "baseaddr", &ip->baseaddr,	// required
		                               "irq",      &ip->irq);		// optional

		if(ret != 0) {
			cpp_warn << "Problem while parsing JSON for IP "
			         << TXT_BOLD(ipName);
			continue;
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
				cpp_error << "Cannot find '" << depName << "' dependency "
				          << depVlnv.toString()
				          << "of " << TXT_BOLD(ipName);
				dependenciesOk = false;
				break;
			}

			cpp_debug << "Found dependency IP " << (*iter)->id;
			ip->dependencies[depName] = (*iter).get();
		}

		if(not dependenciesOk) {
			continue;
		}

		// IP-specific setup via JSON config
		ipCoreFactory->configureJson(*ip, json_ip);

		// TODO: currently fails, fix and remove comment
//		if(not ip->start()) {
//			cpp_error << "Cannot start IP" << ip->id.name;
//			continue;
//		}

		if(not ip->check()) {
			cpp_error << "Checking IP " << ip->id.name << " failed";
			continue;
		}

		initializedIps.push_back(std::move(ip));
	}


	return initializedIps;
}

} // namespace ip
} // namespace fpga
} // namespace villas
