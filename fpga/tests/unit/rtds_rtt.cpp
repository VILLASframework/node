/** RTDS AXI-Stream RTT unit test.
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

#include <villas/fpga/card.hpp>
#include <villas/fpga/vlnv.hpp>

#include <villas/fpga/ips/dma.hpp>
#include <villas/fpga/ips/switch.hpp>
#include <villas/fpga/ips/rtds.hpp>

extern struct fpga_card *card;

// cppcheck-suppress unknownMacro
Test(fpga, rtds_rtt, .description = "RTDS: tight rtt")
{
	int ret;
	struct fpga_ip *ip, *rtds;
	struct dma_mem buf;
	size_t recvlen;

	std::list<villas::fpga::ip::Rtds*> rtdsIps;
	std::list<villas::fpga::ip::Dma*> dmaIps;

	/* Get IP cores */
	for (auto &ip : state.cards.front()->ips) {
		if (*ip == villas::fpga::Vlnv("acs.eonerc.rwth-aachen.de:user:rtds_axis:")) {
			auto rtds = reinterpret_cast<villas::fpga::ip::Rtds*>(ip.get());
			rtdsIps.push_back(rtds);
		}

		if (*ip == villas::fpga::Vlnv("xilinx.com:ip:axi_dma:")) {
			auto dma = reinterpret_cast<villas::fpga::ip::Dma*>(ip.get());
			dmaIps.push_back(dma);
		}
	}

	for (auto rtds : rtdsIps) {
		for (auto dma : dmaIps) {

			rtds->connect

		}
	}

	ret = switch_connect(card->sw, rtds, ip);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_connect(card->sw, ip, rtds);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = dma_alloc(ip, &buf, 0x100, 0);
	cr_assert_eq(ret, 0, "Failed to allocate DMA memory");

	while (1) {

		ret = dma_read(ip, buf.base_phys, buf.len);
		cr_assert_eq(ret, 0, "Failed to start DMA read: %d", ret);

		ret = dma_read_complete(ip, NULL, &recvlen);
		cr_assert_eq(ret, 0, "Failed to complete DMA read: %d", ret);

		ret = dma_write(ip, buf.base_phys, recvlen);
		cr_assert_eq(ret, 0, "Failed to start DMA write: %d", ret);

		ret = dma_write_complete(ip, NULL, NULL);
		cr_assert_eq(ret, 0, "Failed to complete DMA write: %d", ret);
	}

	ret = switch_disconnect(card->sw, rtds, ip);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_disconnect(card->sw, ip, rtds);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = dma_free(ip, &buf);
	cr_assert_eq(ret, 0, "Failed to release DMA memory");
}
