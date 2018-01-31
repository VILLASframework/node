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
#include <villas/fpga/card.hpp>
#include <villas/fpga/ips/fifo.hpp>


extern villas::fpga::PCIeCard* fpga;

Test(fpga, fifo, .description = "FIFO")
{
	ssize_t len;
	char src[255], dst[255];

	auto logger = loggerGetOrCreate("unittest:fifo");

	for(auto& ip : fpga->ips) {
		// skip non-fifo IPs
		if(*ip != villas::fpga::Vlnv("xilinx.com:ip:axi_fifo_mm_s:"))
			continue;

		logger->info("Testing {}", *ip);

		auto fifo = reinterpret_cast<villas::fpga::ip::Fifo&>(*ip);

		if(not fifo.loopbackPossible()) {
			logger->info("Loopback test not possible for {}", *ip);
			continue;
		}

		if(not fifo.connectLoopback()) {
			continue;
		}

		/* Get some random data to compare */
		memset(dst, 0, sizeof(dst));
		len = read_random((char *) src, sizeof(src));
		if (len != sizeof(src)) {
			logger->error("Failed to get random data");
			continue;
		}

		len = fifo.write(src, sizeof(src));
		if (len != sizeof(src)) {
			logger->error("Failed to send to FIFO");
			continue;
		}

		len = fifo.read(dst, sizeof(dst));
		if (len != sizeof(dst)) {
			logger->error("Failed to read from FIFO");
			continue;
		}

		/* Compare data */
		cr_assert_eq(memcmp(src, dst, sizeof(src)), 0, "Data not equal");

		logger->info(TXT_GREEN("Passed"));
	}
}
