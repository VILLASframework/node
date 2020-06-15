/** Block RAM IP.
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
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

#include <villas/fpga/ips/bram.hpp>

using namespace villas::fpga::ip;

static BramFactory factory;

bool
BramFactory::configureJson(Core &ip, json_t* json_ip)
{
	auto &bram = dynamic_cast<Bram&>(ip);

	if (json_unpack(json_ip, "{ s: i }", "size", &bram.size) != 0) {
		getLogger()->error("Cannot parse 'size'");
		return false;
	}

	return true;
}

bool Bram::init()
{
	allocator = std::make_unique<LinearAllocator>
	            (getAddressSpaceId(memoryBlock), this->size, 0);

	return true;
}

