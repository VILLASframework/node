/** Memcpy benchmarks.
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

#include <villas/utils.hpp>

#include <villas/fpga/card.h>

#include <villas/fpga/ips/intc.h>

#include "bench.h"

int fpga_benchmark_memcpy(struct fpga_card *c)
{
	char *map = c->map + 0x200000;
	uint32_t *mapi = (uint32_t *) map;

	char fn[256];
	snprintf(fn, sizeof(fn), "results/bar0_%s_%s.dat", intc_flags & INTC_POLLING ? "polling" : "irq", uts.release);
	FILE *g = fopen(fn, "w");
	fprintf(g, "# bytes cycles\n");

	uint32_t dummy = 0;

	for (int exp = BENCH_DM_EXP_MIN; exp <= BENCH_DM_EXP_MAX; exp++) {
		uint64_t len = 1 << exp;
		uint64_t start, end, total = 0;
		uint64_t runs = (BENCH_RUNS << 2) >> exp;

		for (int i = 0; i < runs + BENCH_WARMUP; i++) {
			start = rdtsc();

			for (int j = 0; j < len / 4; j++)
//				mapi[j] = j;		// write
				dummy += mapi[j];	// read

			end = rdtsc();

			if (i > BENCH_WARMUP)
				total += end - start;
		}

		info("exp = %u\truns = %ju\ttotal = %ju\tavg = %ju\tavgw = %ju", exp, runs, total, total / runs, total / (runs * len));
		fprintf(g, "%zu %lu %ju\n", len, total / runs, runs);
	}

	fclose(g);

	return 0;
}
