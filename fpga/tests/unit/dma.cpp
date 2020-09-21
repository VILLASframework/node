/** DMA unit test.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
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

#include <criterion/criterion.h>

#include <villas/log.hpp>
#include <villas/utils.hpp>
#include <villas/memory.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/bram.hpp>

#include "global.hpp"

using namespace villas;

// cppcheck-suppress unknownMacro
Test(fpga, dma, .description = "DMA")
{
	auto logger = logging.get("unit-test:dma");

	std::list<std::shared_ptr<fpga::ip::Dma>> dmaIps;
	
	for (auto &ip : state.cards.front()->ips) {
		if (*ip == fpga::Vlnv("xilinx.com:ip:axi_dma:")) {
			auto dma = std::dynamic_pointer_cast<fpga::ip::Dma>(ip);
			dmaIps.push_back(dma);
		}
	}

	size_t count = 0;
	for (auto &dma : dmaIps) {
		logger->info("Testing {}", *dma);

		if (not dma->loopbackPossible()) {
			logger->info("Loopback test not possible for {}", *dma);
			continue;
		}

		dma->connectLoopback();

		// Simple DMA can only transfer up to 4 kb due to PCIe page size burst
		// limitation
		size_t len = 4 * (1 << 10);

#if 0
		/* Allocate memory to use with DMA */
		auto src = HostDmaRam::getAllocator().allocate<char>(len);
		auto dst = HostDmaRam::getAllocator().allocate<char>(len);
#elif 0
		/* ... only works with IOMMU enabled currently */

		/* find a block RAM IP to write to */
		auto bramIp = state.cards.front()->lookupIp(fpga::Vlnv("xilinx.com:ip:axi_bram_ctrl:"));
		auto bram = std::dynamic_pointer_cast<fpga::ip::Bram>(bramIp);
		cr_assert_not_null(bram, "Couldn't find BRAM");

		auto src = bram->getAllocator().allocate<char>(len);
		auto dst = bram->getAllocator().allocate<char>(len);
#else
		/* ... only works with IOMMU enabled currently */
		auto src = HostRam::getAllocator().allocate<char>(len);
		auto dst = HostRam::getAllocator().allocate<char>(len);
#endif
		/* Make sure memory is accessible for DMA */
		cr_assert(dma->makeAccesibleFromVA(src.getMemoryBlock()),
		          "Source memory not accessible for DMA");
		cr_assert(dma->makeAccesibleFromVA(dst.getMemoryBlock()),
		          "Destination memory not accessible for DMA");

		/* Get new random data */
		const size_t lenRandom = utils::read_random(&src, len);
		cr_assert(len == lenRandom, "Failed to get random data");

		/* Start transfer */
		cr_assert(dma->memcpy(src.getMemoryBlock(), dst.getMemoryBlock(), len),
		          "DMA ping pong failed");

		/* Compare data */
		cr_assert(memcmp(&src, &dst, len) == 0, "Data not equal");

		logger->info(CLR_GRN("Passed"));

		count++;
	}

	cr_assert(count > 0, "No DMA found");

	MemoryManager::get().getGraph().dump();
	fpga::ip::Node::getGraph().dump();
}
