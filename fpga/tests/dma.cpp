#include <criterion/criterion.h>

#include <villas/log.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/bram.hpp>

#include <villas/utils.h>

#include "global.hpp"

#include <villas/memory.hpp>


Test(fpga, dma, .description = "DMA")
{
	auto logger = loggerGetOrCreate("unittest:dma");

	size_t count = 0;
	for(auto& ip : state.cards.front()->ips) {
		// skip non-dma IPs
		if(*ip != villas::fpga::Vlnv("xilinx.com:ip:axi_dma:"))
			continue;

		logger->info("Testing {}", *ip);

		auto dma = dynamic_cast<villas::fpga::ip::Dma&>(*ip);

		if(not dma.loopbackPossible()) {
			logger->info("Loopback test not possible for {}", *ip);
			continue;
		}

		count++;


		// Simple DMA can only transfer up to 4 kb due to PCIe page size burst
		// limitation
		size_t len = 4 * (1 << 10);

		// find a block RAM IP to write to
		auto bramIp = state.cards.front()->lookupIp(villas::fpga::Vlnv("xilinx.com:ip:axi_bram_ctrl:"));
		auto bram = reinterpret_cast<villas::fpga::ip::Bram*>(bramIp);
		cr_assert_not_null(bram, "Couldn't find BRAM");


		/* Allocate memory to use with DMA */
		auto src = villas::HostDmaRam::getAllocator().allocate<char>(len);
		auto dst = villas::HostDmaRam::getAllocator().allocate<char>(len);

		/* ... only works with IOMMU enabled currently */
//		auto src = bram->getAllocator().allocate<char>(len);
//		auto dst = bram->getAllocator().allocate<char>(len);

		/* ... only works with IOMMU enabled currently */
//		auto src = villas::HostRam::getAllocator().allocate<char>(len);
//		auto dst = villas::HostRam::getAllocator().allocate<char>(len);

		/* Make sure memory is accessible for DMA */
		cr_assert(dma.makeAccesibleFromVA(src.getMemoryBlock()),
		          "Source memory not accessible for DMA");
		cr_assert(dma.makeAccesibleFromVA(dst.getMemoryBlock()),
		          "Destination memory not accessible for DMA");

		/* Get new random data */
		const size_t lenRandom = read_random(&src, len);
		cr_assert(len == lenRandom, "Failed to get random data");


		/* Start transfer */
		cr_assert(dma.memcpy(src.getMemoryBlock(), dst.getMemoryBlock(), len),
		          "DMA ping pong failed");


		/* Compare data */
		cr_assert(memcmp(&src, &dst, len) == 0, "Data not equal");

		logger->info(TXT_GREEN("Passed"));
	}

	villas::MemoryManager::get().dump();

	cr_assert(count > 0, "No Dma found");
}
