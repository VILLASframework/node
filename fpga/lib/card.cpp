/** FPGA card.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <string>
#include <memory>
#include <utility>

#include <villas/exceptions.hpp>
#include <villas/memory.hpp>

#include <villas/kernel/pci.hpp>
#include <villas/kernel/vfio.hpp>

#include <villas/fpga/core.hpp>
#include <villas/fpga/card.hpp>

using namespace villas;
using namespace villas::fpga;

// instantiate factory to register
static PCIeCardFactory villas::fpga::PCIeCardFactory;

static const kernel::pci::Device defaultFilter((kernel::pci::Id(FPGA_PCI_VID_XILINX, FPGA_PCI_PID_VFPGA)));

PCIeCard::List
PCIeCardFactory::make(json_t *json, std::shared_ptr<kernel::pci::DeviceList> pci, std::shared_ptr<kernel::vfio::Container> vc)
{
	PCIeCard::List cards;
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

		if (ret != 0) {
			logger->warn("Cannot parse JSON config");
			continue;
		}

		auto card = std::unique_ptr<PCIeCard>(create());

		// populate generic properties
		card->name = std::string(card_name);
		card->vfioContainer = std::move(vc);
		card->affinity = affinity;
		card->doReset = do_reset != 0;

		kernel::pci::Device filter = defaultFilter;
		
		if (pci_id)
			filter.id = kernel::pci::Id(pci_id);
		if (pci_slot)
			filter.slot = kernel::pci::Slot(pci_slot);

		/* Search for FPGA card */
		card->pdev = pci->lookupDevice(filter);
		if (!card->pdev) {
			logger->warn("Failed to find PCI device");
			continue;
		}

		if (not card->init()) {
			logger->warn("Cannot start FPGA card {}", card_name);
			continue;
		}

		if (not json_is_object(json_ips))
			throw ConfigError(json_ips, "node-config-fpga-ips", "FPGA IP core list must be an object!");

		card->ips = ip::CoreFactory::make(card.get(), json_ips);
		if (card->ips.empty())
			throw ConfigError(json_ips, "node-config-fpga-ips", "Cannot initialize IPs of FPGA card {}", card_name);

		if (not card->check())
			throw RuntimeError("Checking of FPGA card {} failed", card_name);

		cards.push_back(std::move(card));
	}

	return cards;
}

PCIeCard::~PCIeCard()
{
	auto &mm = MemoryManager::get();

	// unmap all memory blocks
	for (auto &mappedMemoryBlock : memoryBlocksMapped) {
		auto translation = mm.getTranslation(addrSpaceIdDeviceToHost,
		                                     mappedMemoryBlock);

		const uintptr_t iova = translation.getLocalAddr(0);
		const size_t size = translation.getSize();

		logger->debug("Unmap block {} at IOVA {:#x} of size {:#x}",
		              mappedMemoryBlock, iova, size);
		vfioContainer->memoryUnmap(iova, size);
	}
}


ip::Core::Ptr
PCIeCard::lookupIp(const std::string &name) const
{
	for (auto &ip : ips) {
		if (*ip == name) {
			return ip;
		}
	}

	return nullptr;
}


ip::Core::Ptr
PCIeCard::lookupIp(const Vlnv &vlnv) const
{
	for (auto &ip : ips) {
		if (*ip == vlnv) {
			return ip;
		}
	}

	return nullptr;
}

ip::Core::Ptr
PCIeCard::lookupIp(const ip::IpIdentifier &id) const
{
	for (auto &ip : ips) {
		if (*ip == id) {
			return ip;
		}
	}

	return nullptr;
}


bool
PCIeCard::mapMemoryBlock(const MemoryBlock &block)
{
	if (not vfioContainer->isIommuEnabled()) {
		logger->warn("VFIO mapping not supported without IOMMU");
		return false;
	}

	auto &mm = MemoryManager::get();
	const auto &addrSpaceId = block.getAddrSpaceId();

	if (memoryBlocksMapped.find(addrSpaceId) != memoryBlocksMapped.end())
		// block already mapped
		return true;
	else
		logger->debug("Create VFIO mapping for {}", addrSpaceId);

	auto translationFromProcess = mm.getTranslationFromProcess(addrSpaceId);
	uintptr_t processBaseAddr = translationFromProcess.getLocalAddr(0);
	uintptr_t iovaAddr = vfioContainer->memoryMap(processBaseAddr,
	                                              UINTPTR_MAX,
	                                              block.getSize());

	if (iovaAddr == UINTPTR_MAX) {
		logger->error("Cannot map memory at {:#x} of size {:#x}",
		              processBaseAddr, block.getSize());
		return false;
	}

	mm.createMapping(iovaAddr, 0, block.getSize(),
	                 "VFIO-D2H",
	                 this->addrSpaceIdDeviceToHost,
	                 addrSpaceId);

	// remember that this block has already been mapped for later
	memoryBlocksMapped.insert(addrSpaceId);

	return true;
}


bool
PCIeCard::init()
{
	logger = getLogger();

	logger->info("Initializing FPGA card {}", name);

	/* Attach PCIe card to VFIO container */
	kernel::vfio::Device &device = vfioContainer->attachDevice(*pdev);
	this->vfioDevice = &device;

	/* Enable memory access and PCI bus mastering for DMA */
	if (not device.pciEnable()) {
		logger->error("Failed to enable PCI device");
		return false;
	}

	/* Reset system? */
	if (doReset) {
		/* Reset / detect PCI device */
		if (not vfioDevice->pciHotReset()) {
			logger->error("Failed to reset PCI device");
			return false;
		}

		if (not reset()) {
			logger->error("Failed to reset FGPA card");
			return false;
		}
	}

	return true;
}
