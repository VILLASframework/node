/** AXI General Purpose IO (GPIO)
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2020, Steffen Vogel
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

#include <xilinx/xintc.h>

#include <villas/fpga/ip.hpp>

namespace villas {
namespace fpga {
namespace ip {


class GeneralPurposeIO : public IpCore
{
public:

	bool init();

private:

	static constexpr char registerMemory[] = "Reg";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{ return { registerMemory }; }
};

class GeneralPurposeIOFactory : public IpCoreFactory {
public:

	static constexpr const char*
	getCompatibleVlnvString()
	{ return "xilinx.com:ip:axi_gpio:"; }

	IpCore* create()
	{ return new GeneralPurposeIO; }

	virtual std::string
	getName() const
	{ return "GeneralPurposeIO"; }

	virtual std::string
	getDescription() const
	{ return "Xilinx's AXI4 general purpose IO"; }

	virtual Vlnv
	getCompatibleVlnv() const
	{ return Vlnv(getCompatibleVlnvString()); }
};

} /* namespace ip */
} /* namespace fpga */
} /* namespace villas */

/** @} */
