/** RTDS AXI-Stream RTT unit test.
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

#include <villas/fpga/card.h>
#include <villas/fpga/vlnv.h>

#include <villas/fpga/ips/dma.h>
#include <villas/fpga/ips/switch.h>
#include <villas/fpga/ips/rtds_axis.h>

extern struct fpga_card *card;

Test(fpga, rtds_rtt, .description = "RTDS: tight rtt")
{
	int ret;
	struct fpga_ip *ip, *rtds;
	struct dma_mem buf;
	size_t recvlen;

	/* Get IP cores */
	rtds = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "acs.eonerc.rwth-aachen.de", "user", "rtds_axis", NULL });
	cr_assert(rtds);

	ip   = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_dma", NULL });
	cr_assert(ip);

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
