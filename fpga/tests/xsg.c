/** System Generator unit test.
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

#include <math.h>

#include <criterion/criterion.h>

#include <villas/log.h>

#include <villas/fpga/card.h>
#include <villas/fpga/ip.h>
#include <villas/fpga/vlnv.h>

#include <villas/fpga/ips/dma.h>

extern struct fpga_card *card;

Test(fpga, xsg, .description = "XSG: multiply_add")
{
	int ret;
	double factor, err = 0;

	struct fpga_ip *ip, *dma;
	struct model_parameter *p;
	struct dma_mem mem;

	ip = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { NULL, "sysgen", "xsg_multiply", NULL });
	cr_assert(ip);

	dma = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_dma", NULL });
	cr_assert(dma);

	struct model *model = (struct model *) ip->_vd;

	p = list_lookup(&model->parameters, "factor");
	if (!p)
		error("Missing parameter 'factor' for model '%s'", ip->name);

	ret = model_parameter_read(p, &factor);
	cr_assert_eq(ret, 0, "Failed to read parameter 'factor' from model '%s'", ip->name);

	info("Model param: factor = %f", factor);

	ret = switch_connect(card->sw, dma, ip);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_connect(card->sw, ip, dma);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = dma_alloc(dma, &mem, 0x1000, 0);
	cr_assert_eq(ret, 0, "Failed to allocate DMA memory");

	float *src = (float *) mem.base_virt;
	float *dst = (float *) mem.base_virt + 0x800;

	for (int i = 0; i < 6; i++)
		src[i] = 1.1 * (i+1);

	ret = dma_ping_pong(dma, (char *) src, (char *) dst, 6 * sizeof(float));
	cr_assert_eq(ret, 0, "Failed to to ping pong DMA transfer: %d", ret);

	for (int i = 0; i < 6; i++)
		err += fabs(factor * src[i] - dst[i]);

	info("Error after FPGA operation: err = %f", err);

	ret = switch_disconnect(card->sw, dma, ip);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_disconnect(card->sw, ip, dma);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = dma_free(dma, &mem);
	cr_assert_eq(ret, 0, "Failed to release DMA memory");

	cr_assert(err < 1e-3);
}
