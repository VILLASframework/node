#include <cstdint>
#include <cstdio>

#include <cuda.h>
#include <cuda_runtime.h>

#include <villas/gpu.hpp>
#include <villas/fpga/ips/rtds2gpu.hpp>



__global__ void
gpu_rtds_rtt_kernel(volatile uint32_t* dataIn, volatile reg_doorbell_t* doorbellIn,
                    volatile uint32_t* dataOut, volatile villas::fpga::ip::ControlRegister* controlRegister,
                    int* run)
{
	printf("[gpu] gpu kernel go\n");

	printf("dataIn:      %p\n", dataIn);
	printf("doorbellIn:  %p\n", doorbellIn);
	printf("dataOut:     %p\n", dataOut);
	printf("control:     %p\n", controlRegister);
	printf("run:         %p\n", run);

//	*run = reinterpret_cast<bool*>(malloc(sizeof(bool)));
//	**run = true;

	uint32_t last_seq;
	while (*run) {
		// wait for data
//		printf("[gpu] wait for data, last_seq=%u\n", last_seq);
		while (not (doorbellIn->is_valid and (last_seq != doorbellIn->seq_nr)) and *run);
//			printf("doorbell: 0x%08x\n", doorbellIn->value);

		last_seq = doorbellIn->seq_nr;

//		printf("[gpu] copy data\n");
		for (size_t i = 0; i < doorbellIn->count; i++) {
			dataOut[i] = dataIn[i];
		}

		// reset doorbell
//		printf("[gpu] reset doorbell\n");
//		doorbellIn->value = 0;

//		printf("[gpu] signal go for gpu2rtds\n");
		controlRegister->ap_start = 1;
	}

	printf("kernel done\n");
}

static int* run = nullptr;

void gpu_rtds_rtt_start(volatile uint32_t* dataIn, volatile reg_doorbell_t* doorbellIn,
                        volatile uint32_t* dataOut, volatile villas::fpga::ip::ControlRegister* controlRegister)
{
	printf("run:         %p\n", run);
	if (run == nullptr) {
		run = (int*)malloc(sizeof(uint32_t));
		cudaHostRegister(run, sizeof(uint32_t), 0);
	}
	printf("run:         %p\n", run);


	*run = 1;
	gpu_rtds_rtt_kernel<<<1, 1>>>(dataIn, doorbellIn, dataOut, controlRegister, run);
	printf("[cpu] kernel launched\n");
}

void gpu_rtds_rtt_stop()
{
	*run = 0;
	cudaDeviceSynchronize();
}
