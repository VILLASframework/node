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
#include "log.h"
#include "plugin.h"

#include "fpga/ip.hpp"

#include <algorithm>
#include <iostream>

#include "log.hpp"

namespace villas {
namespace fpga {
namespace ip {


void IpCore::dump() {
	info("IP %s: vlnv=%s baseaddr=%#jx, irq=%d, port=%d",
	     name.c_str(), vlnv.toString().c_str(), baseaddr, irq, port);
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

IpCore *IpCoreFactory::make(fpga::PCIeCard* card, json_t *json, std::string name)
{
	int ret;
	const char* vlnv_raw;

	// extract VLNV from JSON
	ret = json_unpack(json, "{ s: s }",
	                        "vlnv", &vlnv_raw);
	if(ret != 0) {
		cpp_warn << "IP " << name << " has no entry 'vlnv'";
		return nullptr;
	}

	// parse VLNV
	Vlnv vlnv(vlnv_raw);

	// find the appropriate factory that can create the specified VLNV
	// Note:
	// This is the magic part! Factories automatically register as a plugin
	// as soon as they are instantiated. If there are multiple candidates,
	// the first suitable factory will be used.
	IpCoreFactory* ipCoreFactory = lookup(vlnv);

	if(ipCoreFactory == nullptr) {
		cpp_warn << "No plugin found to handle " << vlnv;
		return nullptr;
	}

	cpp_debug << "Using " << ipCoreFactory->getName() << " for IP " << vlnv;


	// Create new IP instance. Since this function is virtual, it will construct
	// the right, specialized type without knowing it here because we have
	// already picked the right factory.
	IpCore* ip = ipCoreFactory->create();
	if(ip == nullptr) {
		cpp_warn << "Cannot create an instance of " << ipCoreFactory->getName();
		goto fail;
	}

	// setup generic IP type properties
	ip->card = card;
	ip->name = name;
	ip->vlnv = vlnv;

	// extract some optional properties
	ret = json_unpack(json, "{ s?: i, s?: i, s?: i }",
	                        "baseaddr", &ip->baseaddr,
	                        "irq",      &ip->irq,
	                        "port",     &ip->port);
	if(ret != 0) {
		cpp_warn << "Problem while parsing JSON";
		goto fail;
	}

	// IP-specific setup via JSON config
	ipCoreFactory->configureJson(ip, json);

	// TODO: currently fails, fix and remove comment
//	if(not ip->start()) {
//		cpp_error << "Cannot start IP" << ip->name;
//		goto fail;
//	}

	if(not ip->check()) {
		cpp_error << "Checking IP " << ip->name << " failed";
		goto fail;
	}

	return ip;

fail:
	delete ip;
	return nullptr;
}

} // namespace ip
} // namespace fpga
} // namespace villas
