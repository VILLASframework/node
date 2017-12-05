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

namespace villas {


FpgaIpFactory* FpgaIpFactory::lookup(const FpgaVlnv &vlnv)
{
	for(auto& ip : Plugin::lookup(Plugin::Type::FpgaIp)) {
		FpgaIpFactory* fpgaIpFactory = dynamic_cast<FpgaIpFactory *>(ip);

		if(fpgaIpFactory->getCompatibleVlnv() == vlnv)
			return fpgaIpFactory;
	}

	return nullptr;
}

FpgaIp *FpgaIpFactory::make(fpga_card *card, json_t *json, std::string name)
{
	// extract VLNV from JSON
	const char* vlnv_raw;
	if(json_unpack(json, "{ s: s }", "vlnv", &vlnv_raw) != 0)
		error("IP '%s' has no entry 'vlnv'", name.c_str());

	// find the appropriate factory that can create the specified VLNV
	// Note:
	// This is the magic part! Factories automatically register as a plugin
	// as soon as they are instantiated. If there are multiple candidates,
	// the first suitable factory will be used.
	FpgaVlnv vlnv(vlnv_raw);
	FpgaIpFactory* fpgaIpFactory = lookup(vlnv);

	if(fpgaIpFactory == nullptr) {
		error("No ip factory registered to handle VLNV '%s'", vlnv.toString().c_str());
	} else {
		info("Using %s for IP %s", fpgaIpFactory->getName().c_str(), vlnv.toString().c_str());
	}

	// create new IP instance
	FpgaIp* ip = fpgaIpFactory->create();

	// setup generic IP type properties
	ip->card = card;
	ip->name = name;
	ip->vlnv = vlnv;

	// extract some optional properties
	int ret = json_unpack(json, "{ s?: i, s?: i, s?: i }",
	                      "baseaddr", &ip->baseaddr,
	                      "irq",      &ip->irq,
	                      "port",     &ip->port);
	if(ret != 0)
		error("Problem while parsing JSON");

	// IP-specific setup via JSON config
	fpgaIpFactory->configureJson(ip, json);

	if(not ip->start())
		error("Cannot start IP");

	if(not ip->check())
		error("Checking IP failed");

	return ip;
}

} // namespace villas
