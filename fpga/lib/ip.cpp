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

namespace villas {

void FpgaIp::dump() {
	info("IP %s: vlnv=%s baseaddr=%#jx, irq=%d, port=%d",
	     name.c_str(), vlnv.toString().c_str(), baseaddr, irq, port);
}


FpgaIpFactory* FpgaIpFactory::lookup(const FpgaVlnv &vlnv)
{
	for(auto& ip : Plugin::lookup(Plugin::Type::FpgaIp)) {
		FpgaIpFactory* fpgaIpFactory = dynamic_cast<FpgaIpFactory *>(ip);

		if(fpgaIpFactory->getCompatibleVlnv() == vlnv)
			return fpgaIpFactory;
	}

	return nullptr;
}

FpgaIp *FpgaIpFactory::make(FpgaCard* card, json_t *json, std::string name)
{
	int ret;
	const char* vlnv_raw;

	// extract VLNV from JSON
	ret = json_unpack(json, "{ s: s }",
	                        "vlnv", &vlnv_raw);
	if(ret != 0) {
		std::cout << "IP " << name << " has no entry 'vlnv'" << std::endl;
		return nullptr;
	}

	// parse VLNV
	FpgaVlnv vlnv(vlnv_raw);

	// find the appropriate factory that can create the specified VLNV
	// Note:
	// This is the magic part! Factories automatically register as a plugin
	// as soon as they are instantiated. If there are multiple candidates,
	// the first suitable factory will be used.
	FpgaIpFactory* fpgaIpFactory = lookup(vlnv);

	if(fpgaIpFactory == nullptr) {
		std::cout << "No IP plugin registered to handle VLNV " << vlnv << std::endl;
		return nullptr;
	}

	std::cout << "Using " << fpgaIpFactory->getName() << " for IP " << vlnv << std::endl;


	// Create new IP instance. Since this function is virtual, it will construct
	// the right, specialized type without knowing it here because we have
	// already picked the right factory.
	FpgaIp* ip = fpgaIpFactory->create();
	if(ip == nullptr) {
		std::cout << "Cannot create an instance of " << fpgaIpFactory->getName() << std::endl;
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
		std::cout << "Problem while parsing JSON" << std::endl;
		goto fail;
	}

	// IP-specific setup via JSON config
	fpgaIpFactory->configureJson(ip, json);

	// TODO: currently fails, fix and remove comment
//	if(not ip->start()) {
//		std::cout << "Cannot start IP" << ip->name << std::endl;
//		goto fail;
//	}

	if(not ip->check()) {
		std::cout << "Checking IP " << ip->name << " failed" << std::endl;
		goto fail;
	}

	return ip;

fail:
	delete ip;
	return nullptr;
}

} // namespace villas
