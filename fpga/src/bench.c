/** Benchmarks for VILLASfpga
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
#include <string.h>

#include <villas/utils.hpp>
#include <villas/log.h>

#include <villas/fpga/ip.h>
#include <villas/fpga/card.h>
#include <villas/fpga/ips/intc.h>
#include <villas/fpga/ips/timer.h>

#include "bench.h"

#ifdef WITH_LAPACK
int fpga_benchmark_overruns(struct fpga_card *c);
#endif

int intc_flags = 0;
struct utsname uts;

int fpga_benchmarks(int argc, char *argv[], struct fpga_card *c)
{
	int ret;
	struct bench {
		const char *name;
		int (*func)(struct fpga_card *c);
	} benchmarks[] = {
		{ "datamover",	fpga_benchmark_datamover },
		{ "jitter",	fpga_benchmark_jitter },
		{ "memcpy",	fpga_benchmark_memcpy },
#ifdef WITH_LAPACK
		{ "overruns",	fpga_benchmark_overruns },
#endif
		{ "latency",	fpga_benchmark_latency }
	};

	if (argc < 2)
		error("Usage: fpga benchmark (bench)");

	struct bench *bench = NULL;
	for (int i = 0; i < ARRAY_LEN(benchmarks); i++) {
		if (strcmp(benchmarks[i].name, argv[1]) == 0) {
			bench = &benchmarks[i];
			break;
		}
	}

	if (bench == NULL)
		error("There is no benchmark named: %s", argv[1]);

	ret = uname(&uts);
	if (ret)
		return -1;

again:	ret = bench->func(c);
	if (ret)
		error("Benchmark %s failed", bench->name);

	/* Rerun test with polling */
	if (intc_flags == 0) {
		intc_flags |= INTC_POLLING;
		getchar();
		goto again;
	}

	return -1;
}

