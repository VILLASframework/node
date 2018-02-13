/** AXI PCIe bridge
 *
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2018, RWTH Institute for Automation of Complex Power Systems (ACS)
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

#include <limits>
#include <jansson.h>

#include "fpga/ips/pcie.hpp"
#include "fpga/card.hpp"

#include "log.hpp"
#include "memory_manager.hpp"

namespace villas {
namespace fpga {
namespace ip {

static AxiPciExpressBridgeFactory factory;

bool
AxiPciExpressBridge::init()
{
	// Throw an exception if the is no bus master interface and thus no
	// address space we can use for translation -> error
	const MemoryManager::AddressSpaceId myAddrSpaceid =
	        busMasterInterfaces.at(axiInterface);

	// Create an identity mapping from the FPGA card to this IP as an entry
	// point to all other IPs in the FPGA, because Vivado will generate a
	// memory view for this bridge that can see all others.
	MemoryManager::get().createMapping(0x00, 0x00, SIZE_MAX, "PCIeBridge",
	                                   card->addrSpaceId, myAddrSpaceid);

	return true;
}

} // namespace ip
} // namespace fpga
} // namespace villas
