#include <villas/utils.h>
#include <villas/log.h>

#include <villas/fpga/card.h>
#include <villas/fpga/ip.h>

#include <villas/fpga/ips/dma.h>
#include <villas/fpga/ips/switch.h>
#include <villas/fpga/ips/intc.h>

#include "bench.h"

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
#else
  #error "Invalid DM selected"
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
