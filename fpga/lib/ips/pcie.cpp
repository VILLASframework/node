/** AXI PCIe bridge
 *
 * Author: Daniel Krebs <github@daniel-krebs.net>
 * SPDX-FileCopyrightText: 2018 Institute for Automation of Complex Power Systems, EONERC
 * SPDX-License-Identifier: Apache-2.0
 *********************************************************************************/

#include <limits>
#include <jansson.h>

#include <villas/exceptions.hpp>
#include <villas/memory.hpp>

#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/pcie.hpp>
#include <villas/fpga/pcie_card.hpp>

using namespace villas::fpga::ip;

bool
AxiPciExpressBridge::init()
{
	auto &mm = MemoryManager::get();

	// Throw an exception if the is no bus master interface and thus no
	// address space we can use for translation -> error
	card->addrSpaceIdHostToDevice = busMasterInterfaces.at(axiInterface);

	// Map PCIe BAR0 via VFIO
	const void* bar0_mapped = card->vfioDevice->regionMap(VFIO_PCI_BAR0_REGION_INDEX);
	if (bar0_mapped == MAP_FAILED) {
		logger->error("Failed to mmap() BAR0");
		return false;
	}

	// Determine size of BAR0 region
	const size_t bar0_size = card->vfioDevice->regionGetSize(VFIO_PCI_BAR0_REGION_INDEX);

	// Create a mapping from process address space to the FPGA card via vfio
	mm.createMapping(reinterpret_cast<uintptr_t>(bar0_mapped),
	                 0, bar0_size, "vfio-h2d",
	                 mm.getProcessAddressSpace(),
	                 card->addrSpaceIdHostToDevice);

	// Make PCIe (IOVA) address space available to FPGA via BAR0

	// IPs that can access this address space will know it via their memory view
	const auto addrSpaceNameDeviceToHost =
	        mm.getSlaveAddrSpaceName(getInstanceName(), pcieMemory);

	// Save ID in card so we can create mappings later when needed (e.g. when
	// allocating DMA memory in host RAM)
	card->addrSpaceIdDeviceToHost =
	        mm.getOrCreateAddressSpace(addrSpaceNameDeviceToHost);

	auto pciAddrSpaceId = mm.getPciAddressSpace();

	auto regions = dynamic_cast<PCIeCard*>(card)->pdev->getRegions();

	int i = 0;
	for (auto region : regions) {
		const size_t region_size = region.end - region.start + 1;

		char barName[] = "BARx";
		barName[3] = '0' + region.num;
		auto pciBar = pcieToAxiTranslations.at(barName);

		logger->info("PCI-BAR{}: bus addr={:#x} size={:#x}",
		             region.num, region.start, region_size);
		logger->info("PCI-BAR{}: AXI translation offset {:#x}",
		             i, pciBar.translation);

		mm.createMapping(region.start, pciBar.translation, region_size,
		                 std::string("PCI-") + barName,
		                 pciAddrSpaceId, card->addrSpaceIdHostToDevice);
	}

	for (auto& [barName, axiBar] : axiToPcieTranslations) {
		logger->info("AXI-{}: bus addr={:#x} size={:#x}",
		             barName, axiBar.base, axiBar.size);
		logger->info("AXI-{}: PCI translation offset: {:#x}",
		             barName, axiBar.translation);

		auto barXAddrSpaceName = mm.getSlaveAddrSpaceName(getInstanceName(), barName);
		auto barXAddrSpaceId = mm.getOrCreateAddressSpace(barXAddrSpaceName);

		// Base is already incorporated into mapping of each IP by Vivado, so
		// the mapping src has to be 0
		mm.createMapping(0, axiBar.translation, axiBar.size,
		                 std::string("AXI-") + barName,
		                 barXAddrSpaceId, pciAddrSpaceId);

		i++;
	}

	return true;
}

void
AxiPciExpressBridgeFactory::parse(Core &ip, json_t *cfg)
{
	CoreFactory::parse(ip, cfg);

	auto logger = getLogger();
	auto &pcie = dynamic_cast<AxiPciExpressBridge&>(ip);

	for (auto barType : std::list<std::string>{
		"axi_bars",
		"pcie_bars"
	}) {
		json_t *json_bars = json_object_get(cfg, barType.c_str());
		if (not json_is_object(json_bars))
			throw ConfigError(cfg, "", "Missing BAR config: {}", barType);

		json_t* json_bar;
		const char* bar_name;
		json_object_foreach(json_bars, bar_name, json_bar) {
			unsigned int translation;

			json_error_t err;
			int ret = json_unpack_ex(json_bar, &err, 0, "{ s: i }",
				"translation", &translation
			);
			if (ret != 0)
				throw ConfigError(json_bar, err, "", "Cannot parse {}/{}", barType, bar_name);

			if (barType == "axi_bars") {
				json_int_t base, high, size;
				int ret = json_unpack_ex(json_bar, &err, 0, "{ s: I, s: I, s: I }",
				        "baseaddr", &base,
				        "highaddr", &high,
				        "size", &size
				);
				if (ret != 0)
					throw ConfigError(json_bar, err, "", "Cannot parse {}/{}", barType, bar_name);

				pcie.axiToPcieTranslations[bar_name] = {
				    .base = static_cast<uintptr_t>(base),
				    .size = static_cast<size_t>(size),
				    .translation = translation
				};
			} else
				pcie.pcieToAxiTranslations[bar_name] = {
				    .translation = translation
				};
		}
	}
}

static AxiPciExpressBridgeFactory p;
