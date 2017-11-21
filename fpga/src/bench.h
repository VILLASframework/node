#include <sys/utsname.h>

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

extern int intc_flags;
extern struct utsname uts;
