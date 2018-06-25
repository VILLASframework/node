/** HLS unit test.
 *
 * @file
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

#include <unistd.h>

#include <criterion/criterion.h>

#include <villas/fpga/card.h>
#include <villas/fpga/vlnv.h>
#include <villas/fpga/ip.h>

extern struct fpga_card *card;

Test(fpga, hls_dft, .description = "HLS: hls_dft")
{
	int ret;
	struct fpga_ip *hls, *rtds;

	rtds = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "acs.eonerc.rwth-aachen.de", "user", "rtds_axis", NULL });
	hls = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { NULL, "hls", "hls_dft", NULL });

	/* Check if required IP is available on FPGA */
	cr_assert(hls && rtds);

	ret = intc_enable(card->intc, (1 << rtds->irq), 0);
	cr_assert_eq(ret, 0, "Failed to enable interrupt");

	ret = switch_connect(card->sw, rtds, hls);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_connect(card->sw, hls, rtds);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	while(1) {
		/* Dump RTDS AXI Stream state */
		rtds_axis_dump(rtds);
		sleep(1);
	}
#if 0
	int len = 2000;
	int NSAMPLES = 400;
	float src[len], dst[len];

	for (int i = 0; i < len; i++) {
		src[i] = 4 + 5.0 * sin(2.0 * M_PI * 1 * i / NSAMPLES) +
		             2.0 * sin(2.0 * M_PI * 2 * i / NSAMPLES) +
			     1.0 * sin(2.0 * M_PI * 5 * i / NSAMPLES) +
			     0.5 * sin(2.0 * M_PI * 9 * i / NSAMPLES) +
			     0.2 * sin(2.0 * M_PI * 15 * i / NSAMPLES);

		fifo_write()
	}
#endif

	ret = switch_disconnect(card->sw, rtds, hls);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	ret = switch_disconnect(card->sw, hls, rtds);
	cr_assert_eq(ret, 0, "Failed to configure switch");
}
