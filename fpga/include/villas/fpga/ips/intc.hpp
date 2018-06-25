/** AXI-PCIe Interrupt controller
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017-2018, Steffen Vogel
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


class InterruptController : public IpCore
{
public:
	using IrqMaskType = uint32_t;
	static constexpr int maxIrqs = 32;

	~InterruptController();

	bool init();

	bool enableInterrupt(IrqMaskType mask, bool polling);
	bool enableInterrupt(IrqPort irq, bool polling)
	{ return enableInterrupt(1 << irq.num, polling); }

	bool disableInterrupt(IrqMaskType mask);
	bool disableInterrupt(IrqPort irq)
	{ return disableInterrupt(1 << irq.num); }

	int waitForInterrupt(int irq);
	int waitForInterrupt(IrqPort irq)
	{ return waitForInterrupt(irq.num); }

private:

	static constexpr char registerMemory[] = "Reg";

	std::list<MemoryBlockName> getMemoryBlocks() const
	{ return { registerMemory }; }


	struct Interrupt {
		int eventFd;			/**< Event file descriptor */
		int number;				/**< Interrupt number from /proc/interrupts */
		bool polling;			/**< Polled or not */
	};

	int num_irqs;				/**< Number of available MSI vectors */
	int efds[maxIrqs];
	int nos[maxIrqs];
	bool polling[maxIrqs];
};



class InterruptControllerFactory : public IpCoreFactory {
public:

	InterruptControllerFactory() :
	    IpCoreFactory(getName())
	{}

	static constexpr const char*
	getCompatibleVlnvString()
	{ return "acs.eonerc.rwth-aachen.de:user:axi_pcie_intc:"; }

	IpCore* create()
	{ return new InterruptController; }

	std::string
	getName() const
	{ return "InterruptController"; }

	std::string
	getDescription() const
	{ return "Xilinx's programmable interrupt controller"; }

	Vlnv getCompatibleVlnv() const
	{ return Vlnv(getCompatibleVlnvString()); }
};

} // namespace ip
} // namespace fpga
} // namespace villas

/** @} */
