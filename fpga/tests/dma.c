/** DMA unit test.
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

#include <villas/utils.h>

#include <villas/fpga/card.h>
#include <villas/fpga/vlnv.h>
#include <villas/fpga/ip.h>
#include <villas/fpga/ips/dma.h>

#include <villas/log.h>
#include <villas/list.h>

extern struct fpga_card *card;

Test(fpga, dma, .description = "DMA")
{
	int ret = -1;
	struct dma_mem mem, src, dst;

	for (size_t i = 0; i < list_length(&card->ips); i++) { INDENT
		struct fpga_ip *dm = (struct fpga_ip *) list_at(&card->ips, i);

		if (fpga_vlnv_cmp(&dm->vlnv, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_dma", NULL }))
			continue; /* skip non DMA IP cores */

		struct dma *dma = (struct dma *) dm->_vd;

		/* Simple DMA can only transfer up to 4 kb due to
		 * PCIe page size burst limitation */
		ssize_t len2, len = dma->inst.HasSg ? 64 << 20 : 1 << 12;

		ret = dma_alloc(dm, &mem, 2 * len, 0);
		cr_assert_eq(ret, 0);

		ret = dma_mem_split(&mem, &src, &dst);
		cr_assert_eq(ret, 0);

		/* Get new random data */
		len2 = read_random(src.base_virt, len);
		if (len2 != len)
			serror("Failed to get random data");

		int irq_mm2s = dm->irq;
		int irq_s2mm = dm->irq + 1;

		ret = intc_enable(card->intc, (1 << irq_mm2s) | (1 << irq_s2mm), 0);
		cr_assert_eq(ret, 0, "Failed to enable interrupt");

		ret = switch_connect(card->sw, dm, dm);
		cr_assert_eq(ret, 0, "Failed to configure switch");

		/* Start transfer */
		ret = dma_ping_pong(dm, src.base_phys, dst.base_phys, dst.len);
		cr_assert_eq(ret, 0, "DMA ping pong failed");

		ret = memcmp(src.base_virt, dst.base_virt, src.len);

		info("DMA %s (%s): %s", dm->name, dma->inst.HasSg ? "scatter-gather" : "simple", ret ? CLR_RED("failed") : CLR_GRN("passed"));

		ret = switch_disconnect(card->sw, dm, dm);
		cr_assert_eq(ret, 0, "Failed to configure switch");

		ret = intc_disable(card->intc, (1 << irq_mm2s) | (1 << irq_s2mm));
		cr_assert_eq(ret, 0, "Failed to disable interrupt");

		ret = dma_free(dm, &mem);
		cr_assert_eq(ret, 0, "Failed to release DMA memory");
	}

	cr_assert_eq(ret, 0);
}
