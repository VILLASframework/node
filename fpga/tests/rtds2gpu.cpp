/** FIFO unit test.
 *
 * @file
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
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
#include <villas/memory.hpp>
#include <villas/fpga/card.hpp>

#include <villas/fpga/ips/rtds2gpu.hpp>
#include <villas/fpga/ips/switch.hpp>
#include <villas/fpga/ips/dma.hpp>

#include "global.hpp"


static constexpr size_t SAMPLE_SIZE		= 4;
static constexpr size_t SAMPLE_COUNT	= 8;
static constexpr size_t FRAME_SIZE		= SAMPLE_COUNT * SAMPLE_SIZE;


Test(fpga, rtds2gpu, .description = "Rtds2Gpu")
{
	auto logger = loggerGetOrCreate("unittest:rtds2gpu");

	for(auto& ip : state.cards.front()->ips) {
		if(*ip != villas::fpga::Vlnv("xilinx.com:hls:rtds2gpu:"))
			continue;

		logger->info("Testing {}", *ip);

		auto rtds2gpu = reinterpret_cast<villas::fpga::ip::Rtds2Gpu&>(*ip);

		auto dmaMem0 = villas::HostDmaRam::getAllocator(0).allocate<char>(FRAME_SIZE + 4);
		auto dmaMem1 = villas::HostDmaRam::getAllocator(0).allocate<char>(FRAME_SIZE + 4);

//		continue;



		auto axiSwitch = reinterpret_cast<villas::fpga::ip::AxiStreamSwitch*>(
		                     state.cards.front()->lookupIp(villas::fpga::Vlnv("xilinx.com:ip:axis_switch:")));
		cr_assert_not_null(axiSwitch);

		auto dma = reinterpret_cast<villas::fpga::ip::Dma*>(
		                     state.cards.front()->lookupIp(villas::fpga::Vlnv("xilinx.com:ip:axi_dma:")));
		cr_assert_not_null(dma);

		memset(&dmaMem0, 0x55, dmaMem0.getMemoryBlock().getSize());
		memset(&dmaMem1, 0x11, dmaMem1.getMemoryBlock().getSize());

		puts("Before:");
		for(size_t i = 0; i < dmaMem0.getMemoryBlock().getSize(); i++) {
			printf("0x%02x ", dmaMem0[i]);
		}
		puts("");

		rtds2gpu.dump();

		cr_assert(axiSwitch->connect(7, 6));
		cr_assert(axiSwitch->connect(6, 7));



		cr_assert(rtds2gpu.startOnce(dmaMem0.getMemoryBlock(), SAMPLE_COUNT),
		          "Preparing Rtds2Gpu IP failed");

		cr_assert(dma->write(dmaMem1.getMemoryBlock(), FRAME_SIZE));

//		cr_assert(axiSwitch->connect(6, 6)); // loopback
//		cr_assert(dma->read(dmaMem1.getMemoryBlock(), FRAME_SIZE));
//		cr_assert(dma->readComplete());

		cr_assert(dma->writeComplete());

		puts("After:");
		for(size_t i = 0; i < dmaMem0.getMemoryBlock().getSize(); i++) {
			printf("0x%02x ", dmaMem0[i]);
		}
		puts("");


		rtds2gpu.dump();

		cr_assert(rtds2gpu.isDone());

		logger->info(TXT_GREEN("Passed"));
	}
}
