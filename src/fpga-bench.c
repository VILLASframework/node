/** Benchmarks for VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/utsname.h>

#include <villas/utils.h>
#include <villas/hist.h>
#include <villas/log.h>
#include <villas/nodes/fpga.h>
#include <villas/fpga/intc.h>
#include <villas/fpga/ip.h>

#include <xilinx/xtmrctr_l.h>

#include "config.h"
#include "config-fpga.h"

#define BENCH_EXP_MIN	2	// 1 << 2 = 4 Bytes
#define BENCH_EXP_MAX	17	// 1 << 17 = 128 MiB

#define BENCH_ROUNDS	10000
#define BENCH_WARMUP	2000

int fpga_benchmark_datamover(struct fpga *f);
int fpga_benchmark_jitter(struct fpga *f);
int fpga_benchmark_memcpy(struct fpga *f);
int fpga_benchmark_overflows(struct fpga *f);
int fpga_benchmark_latency(struct fpga *f);

enum { POLLING, IRQ } mode;
enum { READ, WRITE } rw = READ;

struct utsname uts;

int fpga_benchmarks(int argc, char *argv[], struct fpga *f)
{
	int ret;
	struct bench {
		const char *name;
		int (*func)(struct fpga *f);
	} benchmarks[] = {
		{ "datamover",	fpga_benchmark_datamover },
		{ "jitter",	fpga_benchmark_jitter },
		{ "memcpy",	fpga_benchmark_memcpy },
		{ "overflows",	fpga_benchmark_overflows },
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

	for (int j = 0; j < 2; j++) {
		mode = j;

		ret = bench->func(f);
		if (ret)
			error("Benchmark %s failed", bench->name);
	}

	return -1;
}

int fpga_benchmark_overflows(struct fpga *f)
{
	struct ip *tmr;

	tmr = list_lookup(&f->ips, "timer_0");
	if (!tmr || !f->intc)
		return -1;

	XTmrCtr *xtmr = &tmr->timer.inst;

	if (mode == IRQ)
		intc_enable(f->intc, (1 << tmr->irq), 0);

	float period = 50e-6;
	int times = period * 1.5 * 1e6; // in uS
	int runs = 1.0 / period;
	int overflows[times];

	XTmrCtr_SetOptions(xtmr, 0, XTC_INT_MODE_OPTION | XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION | XTC_AUTO_RELOAD_OPTION);
	XTmrCtr_SetResetValue(xtmr, 0, period * AXI_HZ);
	XTmrCtr_Start(xtmr, 0);

	for (int p = 0; p < times; p++) {
		overflows[p] = 0;

		for (int i = 0; i < runs; i++) {
			uint32_t tcsr;
			if ((tcsr = XTmrCtr_ReadReg((uintptr_t) f->map + tmr->baseaddr, 0, XTC_TCSR_OFFSET)) & XTC_CSR_INT_OCCURED_MASK)
				overflows[p]++;

			if (mode == IRQ)
				intc_wait(f->intc, tmr->irq, 0);
			else {
				while (!((tcsr = XTmrCtr_ReadReg((uintptr_t) f->map + tmr->baseaddr, 0, XTC_TCSR_OFFSET)) & XTC_CSR_INT_OCCURED_MASK)) __asm__("nop");
			}
			tcsr = XTmrCtr_ReadReg((uintptr_t) f->map + tmr->baseaddr, 0, XTC_TCSR_OFFSET);
			XTmrCtr_WriteReg((uintptr_t) f->map + tmr->baseaddr, 0, XTC_TCSR_OFFSET, tcsr);

			/* Processing */
			rdtsc_sleep(p * 1000, 0);
		}
	}

	XTmrCtr_Stop(xtmr, 0);

	/* Dump results */
	char fn[256];
	snprintf(fn, sizeof(fn), "results/overflows_%s_%s.dat", mode == IRQ ? "irq" : "polling", uts.release);
	FILE *g = fopen(fn, "w");
	fprintf(g, "# period = %f\n", period);
	fprintf(g, "# runs = %u\n", runs);
	for (int i = 0; i < times; i++)
		fprintf(g, "%u\n", overflows[i]);
	fclose(g);

	if (mode == IRQ)
		intc_disable(f->intc, (1 << tmr->irq));
	
	return 0;
}

int fpga_benchmark_jitter(struct fpga *f)
{
	struct ip *tmr;

	tmr = list_lookup(&f->ips, "timer_0");
	if (!tmr || !f->intc)
		return -1;

	XTmrCtr *xtmr = &tmr->timer.inst;

	if (mode == IRQ)
		intc_enable(f->intc, (1 << tmr->irq), 0);
	
	float period = 50e-6;
	int runs = 300.0 / period;
	
	int *hist = (int *) f->dma;

	XTmrCtr_SetOptions(xtmr, 0, XTC_INT_MODE_OPTION | XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION | XTC_AUTO_RELOAD_OPTION);
	XTmrCtr_SetResetValue(xtmr, 0, period * AXI_HZ);
	XTmrCtr_Start(xtmr, 0);

	uint64_t end, start = rdtscp();
	for (int i = 0; i < runs; i++) {
		if (mode == IRQ) {
			uint64_t cnt = intc_wait(f->intc, tmr->irq, 0);
			if (cnt != 1)
				warn("fail");
		}
		else {
			uint32_t tcsr;
			while (!((tcsr = XTmrCtr_ReadReg((uintptr_t) f->map + tmr->baseaddr, 0, XTC_TCSR_OFFSET)) & XTC_CSR_INT_OCCURED_MASK)) __asm__("nop");
			XTmrCtr_WriteReg((uintptr_t) f->map + tmr->baseaddr, 0, XTC_TCSR_OFFSET, tcsr | XTC_CSR_INT_OCCURED_MASK);
		}

		end = rdtscp();
		hist[i] = end - start;
		start = end;
	}

	XTmrCtr_Stop(xtmr, 0);

	char fn[256];
	snprintf(fn, sizeof(fn), "results/jitter_%s_%s.dat", mode == IRQ ? "irq" : "polling", uts.release);
	FILE *g = fopen(fn, "w");
	for (int i = 0; i < runs; i++)
		fprintf(g, "%u\n", hist[i]);
	fclose(g);

	free(hist);
	
	if (mode == IRQ)
		intc_disable(f->intc, (1 << tmr->irq));
	return 0;
}

int fpga_benchmark_latency(struct fpga *f)
{
	uint64_t start, end;

	if (!f->intc)
		return -1;

	int runs = 1000000;
	int hist[runs];

	intc_enable(f->intc, 0x100, mode == POLLING ? 1 : 0);

	for (int i = 0; i < runs; i++) {
		start = rdtscp();
		XIntc_Out32((uintptr_t) f->map + f->intc->baseaddr + XIN_ISR_OFFSET, 0x100);

		intc_wait(f->intc, 8, mode == POLLING ? 1 : 0);
		end = rdtscp();
		
		hist[i] = end - start;
	}

	char fn[256];
	snprintf(fn, sizeof(fn), "results/latency_%s_%s.dat", mode == IRQ ? "irq" : "polling", uts.release);
	FILE *g = fopen(fn, "w");
	for (int i = 0; i < runs; i++)
		fprintf(g, "%u\n", hist[i]);
	fclose(g);

	intc_disable(f->intc, 0x100);

	return 0;
}

int fpga_benchmark_datamover(struct fpga *f)
{
#if 0
	uint64_t start, stop;

	char *src, *dst;
	size_t len;

	for (int exp = BENCH_EXP_MIN; exp < BENCH_EXP_MAX; exp++) {
		size_t sz = 1 << exp;
	
		read_random(src, len);
		memset(dst, 0, len);

		info("Running bench with size: %#zx", sz);

		for (int i = 0; i < BENCH_ROUNDS + BENCH_WARMUP; i++) {
			start = rdtscp();

			switch (bench) {
				case BENCH_FIFO:
					fifo_write(&f->fifo, src, len, irq_fifo);
					fifo_read(&f->fifo, src, len, irq_fifo);
					break;
				case BENCH_DMA_SG:
					dma_write(&f->dma_sg, src, len, irq_dma_mm2s);
					dma_read(&f->dma_sg, src, len, irq_dma_s2mm);
					break;
				case BENCH_DMA_SIMPLE:
					dma_write(&f->dma_simple, src, len, irq_dma_mm2s);
					dma_read(&f->dma_simple, src, len, irq_dma_s2mm);
					break;
			}
			
			stop = rdtscp();

			if (memcmp(src, dst, len))
				warn("Compare failed");

			if (i > BENCH_WARMUP)
				hist_put(&hist, stop - start);
		}
	}
#endif
	return 0;
}

int fpga_benchmark_memcpy(struct fpga *f)
{
#if 1
	char *src = f->map;
	char *dst = alloc(1 << 16);
#else
	char *dst = f->map;
	char *src = alloc(1 << 16);
#endif

	int runs = 10000;
	
	char fn[256];
	snprintf(fn, sizeof(fn), "results/bar0_%s_%s.dat", mode == IRQ ? "irq" : "polling", uts.release);
	FILE *g = fopen(fn, "w");
	fprintf(g, "# runs = %u\n", runs);
	fprintf(g, "# bytes cycles\n");

	size_t len = 1;
	for (int i = 0; i < 16; i++) {
		len = 1 << i;

		uint64_t start, end, total = 0;
		for (int i = 0; i < runs; i++) {
			/* Clear cache */
//			for (char *p = src; p < src + len; p += 64)
//				__builtin_ia32_clflush(p);
//			for (char *p = dst; p < dst + len; p += 64)
//				__builtin_ia32_clflush(p);

			/* Start measurement */
			start = rdtscp();
			memcpy(dst, src, len);
			end = rdtscp();
		
			total += end - start;
		}
		
		fprintf(g, "%zu %lu\n", len, total / runs);
		usleep(100000);
	}
	
	fclose(g);

	return 0;
}