/** Timer/Counter unit test.
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

#include <criterion/criterion.h>

#include "config.h"

#include <villas/log.h>

#include <villas/fpga/card.h>
#include <villas/fpga/vlnv.h>
#include <villas/fpga/ip.h>

#include <villas/fpga/ips/timer.h>

extern struct fpga_card *card;

Test(fpga, timer, .description = "Timer Counter")
{
	int ret;
	struct fpga_ip *ip;
	struct timer *tmr;

	ip = fpga_vlnv_lookup(&card->ips, &(struct fpga_vlnv) { "xilinx.com", "ip", "axi_timer", NULL });
	cr_assert(ip);

	tmr = (struct timer *) ip->_vd;

	XTmrCtr *xtmr = &tmr->inst;

	ret = intc_enable(card->intc, (1 << ip->irq), 0);
	cr_assert_eq(ret, 0, "Failed to enable interrupt");

	XTmrCtr_SetOptions(xtmr, 0, XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION);
	XTmrCtr_SetResetValue(xtmr, 0, FPGA_AXI_HZ / 125);
	XTmrCtr_Start(xtmr, 0);

	uint64_t counter = intc_wait(card->intc, ip->irq);
	info("Got IRQ: counter = %ju", counter);

	if (counter == 1)
		return;
	else
		warn("Counter was not 1");

	intc_disable(card->intc, (1 << ip->irq));
	cr_assert_eq(ret, 0, "Failed to disable interrupt");

	return;
}
