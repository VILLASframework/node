/** GPU unit tests.
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

#include <map>
#include <string>

#include <villas/log.hpp>
#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/bram.hpp>

#include <villas/utils.h>

#include "global.hpp"

#include <villas/memory.hpp>
#include <villas/gpu.hpp>


Test(fpga, gpu_dma, .description = "GPU DMA tests")
{
	auto logger = loggerGetOrCreate("unittest:dma");

	auto& card = state.cards.front();

	villas::Plugin* plugin = villas::Plugin::lookup(villas::Plugin::Type::Gpu, "");
	auto gpuPlugin = dynamic_cast<villas::gpu::GpuFactory*>(plugin);
	cr_assert_not_null(gpuPlugin, "No GPU plugin found");

	auto gpus = gpuPlugin->make();
	cr_assert(gpus.size() > 0, "No GPUs found");

	// just get first cpu
	auto& gpu = gpus.front();

	size_t count = 0;
	for(auto& ip : card->ips) {
		// skip non-dma IPs
		if(*ip != villas::fpga::Vlnv("xilinx.com:ip:axi_bram_ctrl:"))
			continue;

		logger->info("Testing {}", *ip);

		auto bram = reinterpret_cast<villas::fpga::ip::Bram*>(ip.get());
		cr_assert_not_null(bram, "Couldn't find BRAM");

		count++;

		size_t len = 4 * (1 << 10);

		/* Allocate memory to use with DMA */

		auto bram0 = bram->getAllocator().allocate<char>(len);
		auto bram1 = bram->getAllocator().allocate<char>(len);

		gpu->makeAccessibleFromPCIeOrHostRam(bram0.getMemoryBlock());
		gpu->makeAccessibleFromPCIeOrHostRam(bram1.getMemoryBlock());

		auto hostRam0 = villas::HostRam::getAllocator().allocate<char>(len);
		auto hostRam1 = villas::HostRam::getAllocator().allocate<char>(len);

		gpu->makeAccessibleFromPCIeOrHostRam(hostRam0.getMemoryBlock());
		gpu->makeAccessibleFromPCIeOrHostRam(hostRam1.getMemoryBlock());

		auto dmaRam0 = villas::HostDmaRam::getAllocator().allocate<char>(len);
		auto dmaRam1 = villas::HostDmaRam::getAllocator().allocate<char>(len);

		gpu->makeAccessibleFromPCIeOrHostRam(dmaRam0.getMemoryBlock());
		gpu->makeAccessibleFromPCIeOrHostRam(dmaRam1.getMemoryBlock());

		auto gpuMem0 = gpu->getAllocator().allocate<char>(64 << 10);
		auto gpuMem1 = gpu->getAllocator().allocate<char>(64 << 10);

		gpu->makeAccessibleToPCIeAndVA(gpuMem0.getMemoryBlock());
		gpu->makeAccessibleToPCIeAndVA(gpuMem1.getMemoryBlock());


//		auto& src = bram0;
//		auto& dst = bram1;

//		auto& src = hostRam0;
//		auto& dst = hostRam1;

		auto& src = dmaRam0;
//		auto& dst = dmaRam1;

//		auto& src = gpuMem0;
		auto& dst = gpuMem1;


		std::list<std::pair<std::string, std::function<void()>>> memcpyFuncs = {
		    {"cudaMemcpy", [&]() {gpu->memcpySync(src.getMemoryBlock(), dst.getMemoryBlock(), len);}},
		    {"CUDA kernel", [&]() {gpu->memcpyKernel(src.getMemoryBlock(), dst.getMemoryBlock(), len);}},
	    };

		auto dmaIp = card->lookupIp(villas::fpga::Vlnv("xilinx.com:ip:axi_dma:"));
		auto dma = dynamic_cast<villas::fpga::ip::Dma*>(dmaIp);

		if(dma != nullptr and dma->connectLoopback()) {
			memcpyFuncs.push_back({
			    "DMA memcpy", [&]() {
			        if(not dma->makeAccesibleFromVA(src.getMemoryBlock()) or
			           not dma->makeAccesibleFromVA(dst.getMemoryBlock())) {
			            return;
			        }
			        dma->memcpy(src.getMemoryBlock(), dst.getMemoryBlock(), len);
			    }});
		}

		for(auto& [name, memcpyFunc] : memcpyFuncs) {
			logger->info("Testing {}", name);

			/* Get new random data */
			const size_t lenRandom = read_random(&src, len);
			cr_assert(len == lenRandom, "Failed to get random data");

			memcpyFunc();
			const bool success = memcmp(&src, &dst, len) == 0;

			logger->info("  {}", success ?
			                 TXT_GREEN("Passed") :
			                 TXT_RED("Failed"));
		}

		villas::MemoryManager::get().dump();
	}


	cr_assert(count > 0, "No BRAM found");
}
