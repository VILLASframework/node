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

#include <villas/utils.h>

#include <villas/fpga/card.h>
#include <villas/fpga/vlnv.h>
#include <villas/fpga/ip.h>

#include <villas/fpga/ips/fifo.h>

extern struct fpga_card *card;

Test(fpga, fifo, .description = "FIFO")
{
	int ret;
	ssize_t len;
	char src[255], dst[255];
	struct fpga_ip *fifo;

	fifo = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_fifo_mm_s", NULL });
	cr_assert(fifo);

	ret = intc_enable(card->intc, (1 << fifo->irq), 0);
	cr_assert_eq(ret, 0, "Failed to enable interrupt");

	ret = switch_connect(card->sw, fifo, fifo);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	/* Get some random data to compare */
	memset(dst, 0, sizeof(dst));
	len = read_random((char *) src, sizeof(src));
	if (len != sizeof(src))
		error("Failed to get random data");

	len = fifo_write(fifo, (char *) src, sizeof(src));
	if (len != sizeof(src))
		error("Failed to send to FIFO");

	len = fifo_read(fifo, (char *) dst, sizeof(dst));
	if (len != sizeof(dst))
		error("Failed to read from FIFO");

	ret = intc_disable(card->intc, (1 << fifo->irq));
	cr_assert_eq(ret, 0, "Failed to disable interrupt");

	ret = switch_disconnect(card->sw, fifo, fifo);
	cr_assert_eq(ret, 0, "Failed to configure switch");

	/* Compare data */
	cr_assert_eq(memcmp(src, dst, sizeof(src)), 0);
}
