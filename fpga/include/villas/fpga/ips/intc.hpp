/** AXI-PCIe Interrupt controller
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
 * @copyright 2017, Steffen Vogel
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

#include "fpga/ip.hpp"
#include <xilinx/xintc.h>

namespace villas {

class InterruptController : public FpgaIp
{
public:
	using IrqMaskType = uint32_t;
	static constexpr int maxIrqs = 32;

	~InterruptController();

	bool start();

	int enableInterrupt(IrqMaskType mask, bool polling);
	int disableInterrupt(IrqMaskType mask);
	uint64_t waitForInterrupt(int irq);

private:
	struct Interrupt {
		int eventFd;			/**< Event file descriptor */
		int number;				/**< Interrupt number from /proc/interrupts */
		bool polling;			/**< Polled or not */
	};

	int num_irqs;				/**< Number of available MSI vectors */
	int efds[maxIrqs];
	int nos[maxIrqs];
	bool polling[maxIrqs];
//	Interrupt irqs[maxIrqs];	/**< State of available interrupts */
};



class InterruptControllerFactory : public FpgaIpFactory {
public:

	InterruptControllerFactory() :
	    FpgaIpFactory(getName())
	{}

	FpgaIp* create()
	{ return new InterruptController; }

	std::string
	getName() const
	{ return "InterruptController"; }

	std::string
	getDescription() const
	{ return "Xilinx's programmable interrupt controller"; }

	FpgaVlnv getCompatibleVlnv() const
	{ return FpgaVlnv("acs.eonerc.rwth-aachen.de:user:axi_pcie_intc:"); }

	bool configureJson(FpgaIp* ip, json_t *json);
};

} // namespace villas

/** @} */
