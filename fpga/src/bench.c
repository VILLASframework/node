/** Benchmarks for VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Steffen Vogel
 **********************************************************************************/

#include <stdio.h>
#include <string.h>

#include <villas/utils.h>
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

