/** FPGA card.
 *
 * @author Steffen Vogel <post@steffenvogel.de>
 * @copyright 2017-2022, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/kernel/vfio_container.hpp>

#include <villas/fpga/core.hpp>
#include <villas/fpga/node.hpp>
#include <villas/fpga/card.hpp>

using namespace villas;
using namespace villas::fpga;

// Instantiate factory to register
static PCIeCardFactory PCIeCardFactoryInstance;

static const kernel::pci::Device defaultFilter((kernel::pci::Id(FPGA_PCI_VID_XILINX, FPGA_PCI_PID_VFPGA)));

std::list<std::shared_ptr<PCIeCard>> PCIeCardFactory::make(json_t *json, std::shared_ptr<kernel::pci::DeviceList> pci, std::shared_ptr<kernel::vfio::Container> vc)
{
	std::list<std::shared_ptr<PCIeCard>> cards;
	auto logger = getStaticLogger();

	const char *card_name;
	json_t *json_card;
	json_object_foreach(json, card_name, json_card) {
		logger->info("Found config for FPGA card {}", card_name);

		json_t*     json_ips = nullptr;
		json_t*     json_paths = nullptr;
		const char* pci_slot = nullptr;
		const char* pci_id   = nullptr;
		int         do_reset = 0;
		int         affinity = 0;
		int 	    polling  = 0;

		json_error_t err;
		int ret = json_unpack_ex(json_card, &err, 0, "{ s: o, s?: i, s?: b, s?: s, s?: s, s?: b, s?: o }",
			"ips",		&json_ips,
			"affinity", 	&affinity,
			"do_reset", 	&do_reset,
			"slot", 	&pci_slot,
		    	"id", 		&pci_id,
			"polling", 	&polling,
			"paths", 	&json_paths);

		if (ret != 0)
			throw ConfigError(json_card, err, "", "Failed to parse card");

		auto card = std::unique_ptr<PCIeCard>(create());

		// Populate generic properties
		card->name = std::string(card_name);
		card->vfioContainer = vc;
		card->affinity = affinity;
		card->doReset = do_reset != 0;
		card->polling = (polling != 0);

		kernel::pci::Device filter = defaultFilter;

		if (pci_id)
			filter.id = kernel::pci::Id(pci_id);
		if (pci_slot)
			filter.slot = kernel::pci::Slot(pci_slot);

		// Search for FPGA card
		card->pdev = pci->lookupDevice(filter);
		if (!card->pdev) {
			logger->warn("Failed to find PCI device");
			continue;
		}

		if (not card->init()) {
			logger->warn("Cannot start FPGA card {}", card_name);
			continue;
		}

		// Load IPs from a separate json file
		if (json_is_string(json_ips)) {
			auto json_ips_fn = json_string_value(json_ips);
			json_ips = json_load_file(json_ips_fn, 0, nullptr);
			if (json_ips == nullptr)
				throw ConfigError(json_ips, "node-config-fpga-ips", "Failed to load FPGA IP cores from {}", json_ips_fn);
		}

		if (not json_is_object(json_ips))
			throw ConfigError(json_ips, "node-config-fpga-ips", "FPGA IP core list must be an object!");

		card->ips = ip::CoreFactory::make(card.get(), json_ips);
		if (card->ips.empty())
			throw ConfigError(json_ips, "node-config-fpga-ips", "Cannot initialize IPs of FPGA card {}", card_name);

		if (not card->check())
			throw RuntimeError("Checking of FPGA card {} failed", card_name);

		// Additional static paths for AXI-Steram switch
		if (json_paths != nullptr) {
			if (not json_is_array(json_paths))
				throw ConfigError(json_paths, err, "", "Switch path configuration must be an array");

			size_t i;
			json_t *json_path;
			json_array_foreach(json_paths, i, json_path) {
				const char *from, *to;
				int reverse = 0;

				ret = json_unpack_ex(json_path, &err, 0, "{ s: s, s: s, s?: b }",
					"from", &from,
					"to", &to,
					"reverse", &reverse
				);
				if (ret != 0)
					throw ConfigError(json_path, err, "", "Cannot parse switch path config");

				auto masterIpCore = card->lookupIp(from);
				if (!masterIpCore)
					throw ConfigError(json_path, "", "Unknown IP {}", from);

				auto slaveIpCore = card->lookupIp(to);
				if (!slaveIpCore)
					throw ConfigError(json_path, "", "Unknown IP {}", to);

				auto masterIpNode = std::dynamic_pointer_cast<ip::Node>(masterIpCore);
				if (!masterIpNode)
					throw ConfigError(json_path, "", "IP {} is not a streaming node", from);

				auto slaveIpNode = std::dynamic_pointer_cast<ip::Node>(slaveIpCore);
				if (!slaveIpNode)
					throw ConfigError(json_path, "", "IP {} is not a streaming node", to);

				if (not masterIpNode->connect(*slaveIpNode, reverse != 0))
					throw ConfigError(json_path, "", "Failed to connect node {} to {}", from, to);
			}
		}

		cards.push_back(std::move(card));
	}

	return cards;
}

PCIeCard::~PCIeCard()
{
	auto &mm = MemoryManager::get();

	// Unmap all memory blocks
	for (auto &mappedMemoryBlock : memoryBlocksMapped) {
		auto translation = mm.getTranslation(addrSpaceIdDeviceToHost, mappedMemoryBlock);

		const uintptr_t iova = translation.getLocalAddr(0);
		const size_t size = translation.getSize();

		logger->debug("Unmap block {} at IOVA {:#x} of size {:#x}",
		              mappedMemoryBlock, iova, size);
		vfioContainer->memoryUnmap(iova, size);
	}
}

std::shared_ptr<ip::Core> PCIeCard::lookupIp(const std::string &name) const
{
	for (auto &ip : ips) {
		if (*ip == name) {
			return ip;
		}
	}

	return nullptr;
}

std::shared_ptr<ip::Core> PCIeCard::lookupIp(const Vlnv &vlnv) const
{
	for (auto &ip : ips) {
		if (*ip == vlnv) {
			return ip;
		}
	}

	return nullptr;
}

std::shared_ptr<ip::Core> PCIeCard::lookupIp(const ip::IpIdentifier &id) const
{
	for (auto &ip : ips) {
		if (*ip == id) {
			return ip;
		}
	}

	return nullptr;
}

bool PCIeCard::mapMemoryBlock(const MemoryBlock &block)
{
	if (not vfioContainer->isIommuEnabled()) {
		logger->warn("VFIO mapping not supported without IOMMU");
		return false;
	}

	auto &mm = MemoryManager::get();
	const auto &addrSpaceId = block.getAddrSpaceId();

	if (memoryBlocksMapped.find(addrSpaceId) != memoryBlocksMapped.end())
		// Block already mapped
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

	// Remember that this block has already been mapped for later
	memoryBlocksMapped.insert(addrSpaceId);

	return true;
}

bool PCIeCard::init()
{
	logger = getLogger();

	logger->info("Initializing FPGA card {}", name);

	// Attach PCIe card to VFIO container
	vfioDevice = vfioContainer->attachDevice(*pdev);

	// Enable memory access and PCI bus mastering for DMA
	if (not vfioDevice->pciEnable()) {
		logger->error("Failed to enable PCI device");
		return false;
	}

	// Reset system?
	if (doReset) {
		// Reset / detect PCI device
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
