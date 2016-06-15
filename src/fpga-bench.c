/** Benchmarks for VILLASfpga
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2015-2016, Steffen Vogel
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 **********************************************************************************/

#include <stdbool.h>
#include <stdint.h>

#include <villas/utils.h>
#include <villas/hist.h>
#include <villas/log.h>
#include <villas/nodes/vfpga.h>

#include "config.h"
#include "config-fpga.h"

#define BENCH_EXP_MIN	2	// 1 << 2 = 4 Bytes
#define BENCH_EXP_MAX	17	// 1 << 17 = 128 MiB

#define BENCH_ROUNDS	10000
#define BENCH_WARMUP	2000

/* List of available benchmarks */
enum benchmarks {
	BENCH_FIFO,
	BENCH_DMA_SIMPLE,
	BENCH_DMA_SG,
	MAX_BENCHS
};

int fpga_bench(struct vfpga *f, int polling, int bench)
{
	struct hist hist;
	uint64_t start, stop;
	
	hist_create(&hist, 0, 0, 1);

	int irq_fifo     = polling ? -1 : f->vd.msi_efds[MSI_FIFO_MM];
	int irq_dma_mm2s = polling ? -1 : f->vd.msi_efds[MSI_DMA_MM2S];
	int irq_dma_s2mm = polling ? -1 : f->vd.msi_efds[MSI_DMA_S2MM];
	
	char *src, *dst;
	size_t len;

	for (int exp = BENCH_EXP_MIN; exp < BENCH_EXP_MAX; exp++) {
		size_t sz = 1 << exp;
		
		hist_reset(&hist);
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

		hist_dump(&hist);
	}

	hist_destroy(&hist);
}

int fpga_bench_dm(struct vfpga *f)
{
	int irq_fifo, irq_dma_mm2s, irq_dma_s2mm;
	int polling = true;
	int ret;

	/* Loopback all datamovers */
	switch_connect(&f->sw, AXIS_SWITCH_DMA_SIMPLE, AXIS_SWITCH_DMA_SIMPLE);
	switch_connect(&f->sw, AXIS_SWITCH_DMA_SG, AXIS_SWITCH_DMA_SG);
	switch_connect(&f->sw, AXIS_SWITCH_FIFO_MM, AXIS_SWITCH_FIFO_MM);

	for (polling = 0; polling <= 1; polling++) {
		for (int i = 0; i < MAX_BENCHS; i++)
			bench_run(f, polling, i);
	}

	return 0;
}

int fpga_bench_memcpy(struct vfpga *f)
{
	uint64_t start, end, total = 0;

#if 1
	char *src = f->map;
	char *dst = alloc(1 << 13);
#else
	char *dst = f->map;
	char *src = alloc(1 << 13);
	read_random(src, 1 << 13);
#endif

	char *srcb = src;
	for (int i = 0; i < 1 << 13; i++)
		srcb[i] = i;

	unsigned long long runs = 10000;
	
	fgetc(stdin);
	
	//memcpy(dst, src, 0xFF);
	
	for (int i = 0; i < 64; i++)
		dst[i] = src[i];
	
	fgetc(stdin);

	for (int exp = 0; exp < 14; exp++) {
		size_t len = 1 << exp;

		for (int i = 0; i < runs; i++) {
			/* Clear cache */
			for (char *p = src; p < src + len; p += 64)
				__builtin_ia32_clflush(p);

	//		for (char *p = f->dma; p < src + len; p += 64)
	//			__builtin_ia32_clflush(p);

			/* Start measurement */
			start = rdtscp();
			memcpy(dst, src, len);
			end = rdtscp();
		
			total += end - start;
		}
		
		info("avg. clk cycles for %u bytes = %u", len, total / runs);
		usleep(100000);
	}
}