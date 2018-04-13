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

		auto dma = reinterpret_cast<villas::fpga::ip::Dma&>(*ip);

		if(not dma.connectLoopback()) {
			continue;
		}

		count++;

		if(not dma.loopbackPossible()) {
			logger->info("Loopback test not possible for {}", *ip);
			continue;
		}

		// find a block RAM IP to write to
		auto bramIp = state.cards.front()->lookupIp(villas::fpga::Vlnv("xilinx.com:ip:axi_bram_ctrl:"));
		auto bram = reinterpret_cast<villas::fpga::ip::Bram*>(bramIp);
		cr_assert_not_null(bram, "Couldn't find BRAM");

		// Simple DMA can only transfer up to 4 kb due to PCIe page size burst
		// limitation
		size_t len = 4 * (1 << 10);

		/* Allocate memory to use with DMA */
		auto src = villas::HostRam::getAllocator().allocate<char>(len);
		auto dst = bram->getAllocator().allocate<char>(len);

		/* Get new random data */
		const size_t lenRandom = read_random(&src, len);
		cr_assert(len == lenRandom, "Failed to get random data");

		/* Start transfer */
		cr_assert(dma.pingPong(src.getMemoryBlock(), dst.getMemoryBlock(), len),
		          "DMA ping pong failed");

		/* Compare data */
		cr_assert(memcmp(&src, &dst, len) == 0, "Data not equal");

		logger->info(TXT_GREEN("Passed"));
	}

	villas::MemoryManager::get().dump();

	cr_assert(count > 0, "No Dma found");
}
