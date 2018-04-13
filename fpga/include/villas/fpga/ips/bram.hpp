/** Block-Raam related helper functions
 * *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2018, Daniel Krebs
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

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once

#include "memory.hpp"
#include "fpga/ip.hpp"

namespace villas {
namespace fpga {
namespace ip {


class Bram : public IpCore
{
	friend class BramFactory;
public:

	bool init();

	LinearAllocator&
	getAllocator()
	{ return *allocator; }

private:
	static constexpr const char* memoryBlock = "Mem0";
	std::list<MemoryBlockName> getMemoryBlocks() const
	{ return { memoryBlock }; }

	size_t size;
	std::unique_ptr<LinearAllocator> allocator;
};



class BramFactory : public IpCoreFactory {
public:

	BramFactory() :
	    IpCoreFactory(getName())
	{}

	bool configureJson(IpCore& ip, json_t *json_ip);

	IpCore* create()
	{ return new Bram; }

	std::string
	getName() const
	{ return "Bram"; }

	std::string
	getDescription() const
	{ return "Block RAM"; }

	Vlnv getCompatibleVlnv() const
	{ return {"xilinx.com:ip:axi_bram_ctrl:"}; }
};

} // namespace ip
} // namespace fpga
} // namespace villas

/** @} */
