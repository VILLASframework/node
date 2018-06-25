/** Latency benchmarks.
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

#include <stdio.h>

#include <villas/log.h>
#include <villas/utils.h>

#include <villas/fpga/card.h>
#include <villas/fpga/ip.h>

#include "bench.h"

int fpga_benchmark_latency(struct fpga_card *c)
{
	int ret;

	uint64_t start, end;

	if (!c->intc)
		return -1;

	int runs = 1000000;
	int hist[runs];

	ret = intc_enable(c->intc, 0x100, intc_flags);
	if (ret)
		error("Failed to enable interrupts");

	for (int i = 0; i < runs; i++) {
		start = rdtsc();
		XIntc_Out32((uintptr_t) c->map + c->intc->baseaddr + XIN_ISR_OFFSET, 0x100);

		intc_wait(c->intc, 8);
		end = rdtsc();

		hist[i] = end - start;
	}

	char fn[256];
	snprintf(fn, sizeof(fn), "results/latency_%s_%s.dat", intc_flags & INTC_POLLING ? "polling" : "irq", uts.release);
	FILE *g = fopen(fn, "w");
	for (int i = 0; i < runs; i++)
		fprintf(g, "%u\n", hist[i]);
	fclose(g);

	ret = intc_disable(c->intc, 0x100);
	if (ret)
		error("Failed to disable interrupt");

	return 0;
}
