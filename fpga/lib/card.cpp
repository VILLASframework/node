/** FPGA card.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <unistd.h>
#include <string.h>

#include "config.h"
#include "log.h"
#include "log_config.h"
#include "list.h"
#include "utils.h"

#include "kernel/pci.h"
#include "kernel/vfio.h"

#include <string>
#include <map>
#include <algorithm>
#include <list>
#include <vector>
#include <memory>
#include <utility>

#include "fpga/ip.hpp"
#include "fpga/card.hpp"

#include "log.hpp"

namespace villas {
namespace fpga {

// instantiate factory to register
static PCIeCardFactory PCIeCardFactory;


CardList
fpga::PCIeCardFactory::make(json_t *json, struct pci* pci, ::vfio_container* vc)
{
	CardList cards;
	auto logger = getStaticLogger();

	const char *card_name;
	json_t *json_card;
	json_object_foreach(json, card_name, json_card) {
		logger->info("Found config for FPGA card {}", card_name);

		json_t*     json_ips = nullptr;
		const char* pci_slot = nullptr;
		const char* pci_id   = nullptr;
		int         do_reset = 0;
		int         affinity = 0;

		int ret = json_unpack(json_card, "{ s: o, s?: i, s?: b, s?: s, s?: s }",
		    "ips", &json_ips,
		    "affinity", &affinity,
		    "do_reset", &do_reset,
		    "slot", &pci_slot,
		    "id", &pci_id);

		if(ret != 0) {
			logger->warn("Cannot parse JSON config");
			continue;
		}

		auto card = std::unique_ptr<PCIeCard>(create());

		// populate generic properties
		card->name = std::string(card_name);
		card->pci = pci;
		card->vfio_container = vc;
		card->affinity = affinity;
		card->do_reset = do_reset != 0;

		const char* error;

		if (pci_slot != nullptr and pci_device_parse_slot(&card->filter, pci_slot, &error) != 0) {
			logger->warn("Failed to parse PCI slot: {}", error);
		}

		if (pci_id != nullptr and pci_device_parse_id(&card->filter, pci_id, &error) != 0) {
			logger->warn("Failed to parse PCI ID: {}", error);
		}


		// TODO: currently fails, fix and remove comment
		if(not card->init()) {
			logger->warn("Cannot start FPGA card {}", card_name);
			continue;
		}

		card->ips = ip::IpCoreFactory::make(card.get(), json_ips);
		if(card->ips.empty()) {
			logger->error("Cannot initialize IPs of FPGA card {}", card_name);
			continue;
		}

		if(not card->check()) {
			logger->warn("Checking of FPGA card {} failed", card_name);
			continue;
		}

		cards.push_back(std::move(card));
	}

	return cards;
}

fpga::PCIeCard*
fpga::PCIeCardFactory::create()
{
	return new fpga::PCIeCard;
}


ip::IpCore*
PCIeCard::lookupIp(const std::string& name) const
{
	for(auto& ip : ips) {
		if(*ip == name) {
			return ip.get();
		}
	}
	return nullptr;
}


bool fpga::PCIeCard::init()
{
	int ret;
	struct pci_device *pdev;

	auto logger = getLogger();

	/* Search for FPGA card */
	pdev = pci_lookup_device(pci, &filter);
	if (!pdev) {
		logger->error("Failed to find PCI device");
		return false;
	}

	/* Attach PCIe card to VFIO container */
	ret = ::vfio_pci_attach(&vfio_device, vfio_container, pdev);
	if (ret) {
		logger->error("Failed to attach VFIO device");
		return false;
	}

	/* Map PCIe BAR */
	map = (char*) vfio_map_region(&vfio_device, VFIO_PCI_BAR0_REGION_INDEX);
	if (map == MAP_FAILED) {
		logger->error("Failed to mmap() BAR0");
		return false;
	}

	/* Enable memory access and PCI bus mastering for DMA */
	ret = vfio_pci_enable(&vfio_device);
	if (ret) {
		logger->error("Failed to enable PCI device");
		return false;
	}

	/* Reset system? */
	if (do_reset) {
		/* Reset / detect PCI device */
		ret = vfio_pci_reset(&vfio_device);
		if (ret) {
			logger->error("Failed to reset PCI device");
			return false;
		}

		if(not reset()) {
			logger->error("Failed to reset FGPA card");
			return false;
		}
	}

	return true;
}

} // namespace fpga
} // namespace villas
