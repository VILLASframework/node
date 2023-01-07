/** Block RAM IP.
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <villas/exceptions.hpp>
#include <villas/fpga/ips/bram.hpp>

using namespace villas;
using namespace villas::fpga::ip;

void BramFactory::parse(Core &ip, json_t* cfg)
{
	CoreFactory::parse(ip, cfg);

	auto &bram = dynamic_cast<Bram&>(ip);

	json_error_t err;
	int ret = json_unpack_ex(cfg, &err, 0, "{ s: i }",
		"size", &bram.size
	);
	if (ret != 0)
		throw ConfigError(cfg, err, "", "Cannot parse BRAM config");
}

bool Bram::init()
{
	allocator = std::make_unique<LinearAllocator>
	            (getAddressSpaceId(memoryBlock), this->size, 0);

	return true;
}

static BramFactory f;
