/** Block RAM IP.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2022, Institute for Automation of Complex Power Systems, EONERC
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

#include <villas/exceptions.hpp>
#include <villas/fpga/ips/bram.hpp>

using namespace villas::fpga::ip;

static BramFactory factory;

void BramFactory::parse(Core &ip, json_t* cfg)
{
	CoreFactory::parse(ip, cfg);

	auto &bram = dynamic_cast<Bram&>(ip);

	json_error_t err;
	auto ret = json_unpack_ex(cfg, &err, 0, "{ s: i }",
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

