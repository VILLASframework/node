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
	auto& mm = MemoryManager::get();

	// Throw an exception if the is no bus master interface and thus no
	// address space we can use for translation -> error
	const MemoryManager::AddressSpaceId myAddrSpaceid =
	        busMasterInterfaces.at(axiInterface);

	// Create an identity mapping from the FPGA card to this IP as an entry
	// point to all other IPs in the FPGA, because Vivado will generate a
	// memory view for this bridge that can see all others.
	MemoryManager::get().createMapping(0x00, 0x00, SIZE_MAX, "PCIeBridge",
	                                   card->addrSpaceIdHostToDevice, myAddrSpaceid);


	/* Make PCIe (IOVA) address space available to FPGA via BAR0 */

	// IPs that can access this address space will know it via their memory view
	const auto addrSpaceNameDeviceToHost =
	        mm.getSlaveAddrSpaceName(getInstanceName(), pcieMemory);

	// save ID in card so we can create mappings later when needed (e.g. when
	// allocating DMA memory in host RAM)
	card->addrSpaceIdDeviceToHost =
	        mm.getOrCreateAddressSpace(addrSpaceNameDeviceToHost);

	auto pciAddrSpaceId = mm.getPciAddressSpace();

	struct pci_region* pci_regions = nullptr;
	size_t num_regions = pci_get_regions(card->pdev, &pci_regions);

	for(size_t i = 0; i < num_regions; i++) {
		const size_t region_size = pci_regions[i].end - pci_regions[i].start + 1;

		char barName[] = "BARx";
		barName[3] = '0' + pci_regions[i].num;
		auto pciBar = pcieToAxiTranslations.at(barName);


		logger->info("PCI-BAR{}: bus addr={:#x} size={:#x}",
		             pci_regions[i].num, pci_regions[i].start, region_size);
		logger->info("PCI-BAR{}: AXI translation offset {:#x}",
		             i, pciBar.translation);

		mm.createMapping(pci_regions[i].start, pciBar.translation, region_size,
		                 std::string("PCI-") + barName,
		                 pciAddrSpaceId, card->addrSpaceIdHostToDevice);

	}

	if(pci_regions != nullptr) {
		logger->debug("freeing pci regions");
		free(pci_regions);
	}


	for(auto& [barName, axiBar] : axiToPcieTranslations) {
		logger->info("AXI-{}: bus addr={:#x} size={:#x}",
		             barName, axiBar.base, axiBar.size);
		logger->info("AXI-{}: PCI translation offset: {:#x}",
		             barName, axiBar.translation);

		auto barXAddrSpaceName = mm.getSlaveAddrSpaceName(getInstanceName(), barName);
		auto barXAddrSpaceId = mm.getOrCreateAddressSpace(barXAddrSpaceName);

		// base is already incorporated into mapping of each IP by Vivado, so
		// the mapping src has to be 0
		mm.createMapping(0, axiBar.translation, axiBar.size,
		                 std::string("AXI-") + barName,
		                 barXAddrSpaceId, pciAddrSpaceId);
	}

	return true;
}

bool
AxiPciExpressBridgeFactory::configureJson(IpCore& ip, json_t* json_ip)
{
	auto logger = getLogger();
	auto& pcie = reinterpret_cast<AxiPciExpressBridge&>(ip);

	for(auto barType : std::list<std::string>{"axi_bars", "pcie_bars"}) {
		json_t* json_bars = json_object_get(json_ip, barType.c_str());
		if(not json_is_object(json_bars)) {
			return false;
		}

		json_t* json_bar;
		const char* bar_name;
		json_object_foreach(json_bars, bar_name, json_bar) {
			unsigned int translation;
			int ret = json_unpack(json_bar, "{ s: i }", "translation", &translation);
			if(ret != 0) {
				logger->error("Cannot parse {}/{}", barType, bar_name);
				return false;
			}

			if(barType == "axi_bars") {
				json_int_t base, high, size;
				int ret = json_unpack(json_bar, "{ s: I, s: I, s: I }",
				                      "baseaddr", &base,
				                      "highaddr", &high,
				                      "size", &size);
				if(ret != 0) {
					logger->error("Cannot parse {}/{}", barType, bar_name);
					return false;
				}

				pcie.axiToPcieTranslations[bar_name] = {
				    .base = static_cast<uintptr_t>(base),
				    .size = static_cast<size_t>(size),
				    .translation = translation
				};

			} else {
				pcie.pcieToAxiTranslations[bar_name] = {
				    .translation = translation
				};
			}
		}
	}

	return true;
}

} // namespace ip
} // namespace fpga
} // namespace villas
