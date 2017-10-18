/** Benchmarks for VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Steffen Vogel
 **********************************************************************************/

#include <stdio.h>
#include <string.h>
#include <sys/utsname.h>

#include <villas/utils.h>
#include <villas/log.h>
#include <villas/timing.h>

#include <villas/fpga/ip.h>
#include <villas/fpga/card.h>
#include <villas/fpga/ips/intc.h>
#include <villas/fpga/ips/timer.h>

#include "config.h"

/* Some hard-coded configuration for the FPGA benchmarks */
#define BENCH_DM		3
// 1 FIFO
// 2 DMA SG
// 3 DMA Simple

#define BENCH_RUNS		3000000
#define BENCH_WARMUP		100
#define BENCH_DM_EXP_MIN	0
#define BENCH_DM_EXP_MAX	20

int fpga_benchmark_datamover(struct fpga_card *c);
int fpga_benchmark_jitter(struct fpga_card *c);
int fpga_benchmark_memcpy(struct fpga_card *c);
int fpga_benchmark_latency(struct fpga_card *c);

#if defined(WITH_BLAS) && defined(WITH_LAPACK)
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
#if defined(WITH_BLAS) && defined(WITH_LAPACK)
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

int fpga_benchmark_jitter(struct fpga_card *c)
{
	int ret;

	struct fpga_ip *ip = list_lookup(&c->ips, "timer_0");
	if (!ip || !c->intc)
		return -1;

	struct timer *tmr = (struct timer *) ip->_vd;

	XTmrCtr *xtmr = &tmr->inst;

	ret = intc_enable(c->intc, (1 << ip->irq), intc_flags);
	if (ret)
		error("Failed to enable interrupt");

	float period = 50e-6;
	int runs = 300.0 / period;

	int *hist = alloc(8 << 20);

	XTmrCtr_SetOptions(xtmr, 0, XTC_INT_MODE_OPTION | XTC_EXT_COMPARE_OPTION | XTC_DOWN_COUNT_OPTION | XTC_AUTO_RELOAD_OPTION);
	XTmrCtr_SetResetValue(xtmr, 0, period * FPGA_AXI_HZ);
	XTmrCtr_Start(xtmr, 0);

	uint64_t end, start = rdtsc();
	for (int i = 0; i < runs; i++) {
		uint64_t cnt = intc_wait(c->intc, ip->irq);
		if (cnt != 1)
			warn("fail");

		/* Ackowledge IRQ */
		XTmrCtr_WriteReg((uintptr_t) c->map + ip->baseaddr, 0, XTC_TCSR_OFFSET, XTmrCtr_ReadReg((uintptr_t) c->map + ip->baseaddr, 0, XTC_TCSR_OFFSET));

		end = rdtsc();
		hist[i] = end - start;
		start = end;
	}

	XTmrCtr_Stop(xtmr, 0);

	char fn[256];
	snprintf(fn, sizeof(fn), "results/jitter_%s_%s.dat", intc_flags & INTC_POLLING ? "polling" : "irq", uts.release);
	FILE *g = fopen(fn, "w");
	for (int i = 0; i < runs; i++)
		fprintf(g, "%u\n", hist[i]);
	fclose(g);

	free(hist);

	ret = intc_disable(c->intc, (1 << ip->irq));
	if (ret)
		error("Failed to disable interrupt");

	return 0;
}

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

int fpga_benchmark_datamover(struct fpga_card *c)
{
	int ret;

	struct fpga_ip *dm;
	struct dma_mem mem, src, dst;

#if BENCH_DM == 1
	char *dm_name = "fifo_mm_s_0";
#elif BENCH_DM == 2
	char *dm_name = "dma_0";
#elif BENCH_DM == 3
	char *dm_name = "dma_1";
#endif

	dm = list_lookup(&c->ips, dm_name);
	if (!dm)
		error("Unknown datamover");

	ret = switch_connect(c->sw, dm, dm);
	if (ret)
		error("Failed to configure switch");

	ret = intc_enable(c->intc, (1 << dm->irq) | (1 << (dm->irq + 1)), intc_flags);
	if (ret)
		error("Failed to enable interrupt");

	/* Allocate DMA memory */
	ret = dma_alloc(dm, &mem, 2 * (1 << BENCH_DM_EXP_MAX), 0);
	if (ret)
		error("Failed to allocate DMA memory");

	ret = dma_mem_split(&mem, &src, &dst);
	if (ret)
		return -1;

	/* Open file for results */
	char fn[256];
	snprintf(fn, sizeof(fn), "results/datamover_%s_%s_%s.dat", dm_name, intc_flags & INTC_POLLING ? "polling" : "irq", uts.release);
	FILE *g = fopen(fn, "w");

	for (int exp = BENCH_DM_EXP_MIN; exp <= BENCH_DM_EXP_MAX; exp++) {
		uint64_t start, stop, total = 0, len = 1 << exp;

#if BENCH_DM == 1
		if (exp > 11)
			break; /* FIFO and Simple DMA are limited to 4kb */
#elif BENCH_DM == 3
		if (exp >= 12)
			break; /* FIFO and Simple DMA are limited to 4kb */
#endif

		read_random(src.base_virt, len);
		memset(dst.base_virt, 0, len);

		info("Start DM bench: len=%#jx", len);

		uint64_t runs = BENCH_RUNS >> exp;
		for (int i = 0; i < runs + BENCH_WARMUP; i++) {
			start = rdtsc();
#if BENCH_DM == 1
			ssize_t ret;

			ret = fifo_write(dm, src.base_virt, len);
			if (ret < 0)
				error("Failed write to FIFO with len = %zu", len);

			ret = fifo_read(dm, dst.base_virt, dst.len);
			if (ret < 0)
				error("Failed read from FIFO with len = %zu", len);
#else
			ret = dma_ping_pong(dm, src.base_phys, dst.base_phys, len);
			if (ret)
				error("DMA ping pong failed");
#endif
			stop = rdtsc();

			if (memcmp(src.base_virt, dst.base_virt, len))
				warn("Compare failed");

			if (i > BENCH_WARMUP)
				total += stop - start;
		}

		info("exp %u avg %lu", exp, total / runs);
		fprintf(g, "%lu %lu\n", len, total / runs);
	}

	fclose(g);

	ret = switch_disconnect(c->sw, dm, dm);
	if (ret)
		error("Failed to configure switch");

	ret = dma_free(dm, &mem);
	if (ret)
		error("Failed to release DMA memory");

	ret = intc_disable(c->intc, (1 << dm->irq) | (1 << (dm->irq + 1)));
	if (ret)
		error("Failed to enable interrupt");


	return 0;
}

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
