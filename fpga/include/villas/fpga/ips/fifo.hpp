/** Timer related helper functions
 *
 * These functions present a simpler interface to Xilinx' Timer Counter driver (XTmrCtr_*)
 *
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

#include "fpga/ip_node.hpp"
#include <xilinx/xllfifo.h>


namespace villas {
namespace fpga {
namespace ip {


class Fifo : public IpNode
{
public:
	friend class FifoFactory;

	bool start();
	bool stop();

	size_t write(const void* buf, size_t len);
	size_t read(void* buf, size_t len);

private:
	XLlFifo xFifo;
	uintptr_t baseaddr_axi4;
};



class FifoFactory : public IpNodeFactory {
public:
	FifoFactory() :
	    IpNodeFactory(getName())
	{}

	bool configureJson(IpCore& ip, json_t *json_ip);

	IpCore* create()
	{ return new Fifo; }

	std::string
	getName() const
	{ return "Fifo"; }

	std::string
	getDescription() const
	{ return "Xilinx's AXI4 FIFO data mover"; }

	Vlnv getCompatibleVlnv() const
	{ return {"xilinx.com:ip:axi_fifo_mm_s:"}; }

	std::list<IpDependency> getDependencies() const
	{ return { {"intc", Vlnv("acs.eonerc.rwth-aachen.de:user:axi_pcie_intc:") } }; }
};

} // namespace ip
} // namespace fpga
} // namespace villas

/** @} */
