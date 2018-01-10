/** FPGA card
 *
 * This class represents a FPGA device.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @author Daniel Krebs <github@daniel-krebs.net>
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

/** @addtogroup fpga VILLASfpga
 * @{
 */

#pragma once


#include <jansson.h>

#include "common.h"
#include "kernel/pci.h"
#include "kernel/vfio.h"

#include <list>
#include <string>

#include "plugin.hpp"
#include "fpga/ip.hpp"

#include "config.h"

#define PCI_FILTER_DEFAULT_FPGA {		\
	.id = {								\
	    .vendor = FPGA_PCI_VID_XILINX,	\
	    .device = FPGA_PCI_PID_VFPGA	\
	}									\
}

namespace villas {
namespace fpga {


/* Forward declarations */
struct vfio_container;
class PCIeCardFactory;

class PCIeCard {
public:

	friend PCIeCardFactory;

	PCIeCard() : filter(PCI_FILTER_DEFAULT_FPGA) {}

	bool start();
	bool stop()  { return true; }
	bool check() { return true; }
	bool reset() { return true; }
	void dump()  { }

	ip::IpCore* lookupIp(std::string name) const;

	ip::IpCoreList ips;		///< IPs located on this FPGA card

	bool do_reset;			/**< Reset VILLASfpga during startup? */
	int affinity;			/**< Affinity for MSI interrupts */


	std::string name;			/**< The name of the FPGA card */

	struct pci *pci;
	struct pci_device filter;		/**< Filter for PCI device. */

	::vfio_container *vfio_container;
	struct vfio_device vfio_device;	/**< VFIO device handle. */

	char *map;			/**< PCI BAR0 mapping for register access */

	size_t maplen;
	size_t dmalen;

protected:
	SpdLogger
	getLogger() const
	{ return loggerGetOrCreate(name); }
};

using CardList = std::list<std::unique_ptr<PCIeCard>>;

class PCIeCardFactory : public Plugin {
public:

	PCIeCardFactory() :
	    Plugin(Plugin::Type::FpgaCard, "FPGA Card plugin") {}

	static CardList
	make(json_t *json, struct pci* pci, ::vfio_container* vc);

	static PCIeCard*
	create();

	static SpdLogger
	getStaticLogger()
	{ return loggerGetOrCreate("PCIeCardFactory"); }
};

} // namespace fpga
} // namespace villas

/** @} */
