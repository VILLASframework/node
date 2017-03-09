/** Benchmarks for VILLASfpga: LAPACK & BLAS
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2016, Steffen Vogel
 **********************************************************************************/

#include <stdio.h>
#include <sys/utsname.h>

#include <villas/log.h>
#include <villas/nodes/fpga.h>

#include "config.h"

/* Declared in fpga-bench.c */
extern int intc_flags;
extern struct utsname uts;

/* LAPACK & BLAS Fortran prototypes */
extern int dgemm_(char *transa, char *transb, int *m, int *n, int *k, double *alpha, double *a, int *lda, double *b, int *ldb, double *beta, double *c, int *ldc);
extern int dgetrf_(int *m, int *n, double *a, int *lda, int *ipiv, int *info);
extern int dgetri_(int *n, double *a, int *lda, int *ipiv, double *work, int *lwork, int *info);

static int lapack_generate_workload(int N, double *C)
{
	double *A = alloc(N * N * sizeof(double));

	srand(time(NULL));

	for (int i = 0; i < N * N; i++)
		A[i] = 100 * (double) rand() / RAND_MAX + 1;

	char transA = 'T';
	char transB = 'N';
	double alpha = 1;
	double beta = 1;

	/* C = A' * A, to get an invertible matrix */
	dgemm_(&transA, &transB, &N, &N, &N, &alpha, A, &N, A, &N, &beta, C, &N);

	free(A);

	return 0;
}

static int lapack_workload(int N, double *A)
{
	int info = 0; 
	int lworkspace = N;
	int ipiv[N]; 
	double workspace[N]; 

	dgetrf_(&N, &N, A, &N, ipiv, &info);
	if (info > 0)
		error("Failed to pivot matrix");
	
	dgetri_(&N, A, &N, ipiv, workspace, &lworkspace, &info);
	if (info > 0)
		error("Failed to LU factorized matrix");

	return 0;
}

int fpga_benchmark_overruns(struct fpga *f)
{
	struct ip *rtds, *dm;

	dm = list_lookup(&f->ips, "dma_1");
	rtds = list_lookup(&f->ips, "rtds_axis_0");
	if (!rtds || !f->intc)
		return -1;

	int ret;
	float period = 50e-6;
	int runs = 1.0 / period;
	int overruns;
	
	info("runs = %u", runs);

	switch_connect(f->sw, dm, rtds);
	switch_connect(f->sw, rtds, dm);

	intc_enable(f->intc, (1 << (dm->irq + 1  )), intc_flags);

	/* Dump results */
	char fn[256];
	snprintf(fn, sizeof(fn), "results/overruns_lu_rtds_axis_%s_%s.dat", intc_flags & INTC_POLLING ? "polling" : "irq", uts.release);
	FILE *g = fopen(fn, "w");
	fprintf(g, "# period = %f\n", period);
	fprintf(g, "# runs = %u\n", runs);
	
	struct dma_mem mem;
	ret = dma_alloc(dm, &mem, 0x1000, 0);
	if (ret)
		error("Failed to allocate DMA memory");

	uint32_t *data_rx = (uint32_t *) mem.base_virt;
	uint32_t *data_tx = (uint32_t *) mem.base_virt + 0x200;
	uint64_t total, start, stop;
	for (int p = 3; p < 45; p++) {
		double *A = alloc(p*p*sizeof(double));

		lapack_generate_workload(p, A);

		overruns = 0;
		total = 0;
		
		for (int i = 0; i < 2000; i++) {
			dma_read(dm, mem.base_phys, 0x200);
			dma_read_complete(dm, NULL, NULL);
		}

		for (int i = 0; i < runs + BENCH_WARMUP; i++) {
			dma_read(dm, mem.base_phys, 0x200);

			start = rdtsc();
			lapack_workload(p, A);
			stop = rdtsc();

			dma_read_complete(dm, NULL, NULL);

			/* Send data to rtds */
			data_tx[0] = i;
			dma_write(dm, mem.base_phys + 0x200, 64 * sizeof(data_tx[0]));

			if (i < BENCH_WARMUP)
				continue;

			if (i - data_rx[0] > 2)
				overruns++;
			total += stop - start;
		}

		free(A);

		info("iter = %u clks = %ju overruns = %u", p, total / runs, overruns);
		fprintf(g, "%u %ju %u\n", p, total / runs, overruns);
		
		if (overruns >= runs)
			break;
	}

	fclose(g);
	return 0;
}
