/** Intc unit test.
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
#include <villas/fpga/core.hpp>

#include <villas/fpga/ips/intc.hpp>

extern struct fpga_card *card;

Test(fpga, intc, .description = "Interrupt Controller")
{
	int ret;
	uint32_t isr;

	cr_assert(card->intc);

	ret = intc_enable(card->intc, 0xFF00, 0);
	cr_assert_eq(ret, 0, "Failed to enable interrupt");

	/* Fake IRQs in software by writing to ISR */
	XIntc_Out32((uintptr_t) card->map + card->intc->baseaddr + XIN_ISR_OFFSET, 0xFF00);

	/* Wait for 8 SW triggered IRQs */
	for (int i = 0; i < 8; i++)
		intc_wait(card->intc, i+8);

	/* Check ISR if all SW IRQs have been deliverd */
	isr = XIntc_In32((uintptr_t) card->map + card->intc->baseaddr + XIN_ISR_OFFSET);

	ret = intc_disable(card->intc, 0xFF00);
	cr_assert_eq(ret, 0, "Failed to disable interrupt");

	cr_assert_eq(isr & 0xFF00, 0); /* ISR should get cleared by MSI_Grant_signal */
}
