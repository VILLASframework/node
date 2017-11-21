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
