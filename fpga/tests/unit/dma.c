/* Testing the C bindings for the VILLASfpga DMA interface.
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <villas/fpga/dma.h>

int main(int argc, char *argv[])
{
	int ret;
	villasfpga_handle vh;
	villasfpga_memory mem1, mem2;
	void *mem1ptr, *mem2ptr;
	size_t size;

	if (argv == NULL) {
		return 1;
	}
	if (argc != 2 || argv[1] == NULL) {
		fprintf(stderr, "Usage: %s <config file>\n", argv[0]);
	}

	if ((vh = villasfpga_init(argv[1])) == NULL) {
		fprintf(stderr, "Failed to initialize FPGA\n");
		ret = 1;
		goto out;
	}

	if (villasfpga_alloc(vh, &mem1, 0x200 * sizeof(uint32_t)) != 0) {
		fprintf(stderr, "Failed to allocate DMA memory 1\n");
		ret = 1;
		goto out;
	}

	if (villasfpga_alloc(vh, &mem2, 0x200 * sizeof(uint32_t)) != 0) {
		fprintf(stderr, "Failed to allocate DMA memory 2\n");
		ret = 1;
		goto out;
	}

	if ((mem1ptr = villasfpga_get_ptr(mem1)) == NULL) {
		fprintf(stderr, "Failed to get pointer to DMA memory 1\n");
		ret = 1;
		goto out;
	}

	if ((mem2ptr = villasfpga_get_ptr(mem2)) == NULL) {
		fprintf(stderr, "Failed to get pointer to DMA memory 2\n");
		ret = 1;
		goto out;
	}

	printf("DMA memory 1: %p, DMA memory 2: %p\n", mem1ptr, mem2ptr);

	while (1) {
		// Setup read transfer
		if ((ret = villasfpga_read(vh, mem1, 0x200 * sizeof(uint32_t))) != 0) {
			fprintf(stderr, "Failed to initiate read transfer\n");
			ret = 1;
			goto out;
		}

		printf("Enter a float:\n");
		if ((ret = scanf("%f", (float*)mem2ptr)) != 1) {
			fprintf(stderr, "Failed to parse input: sscanf returned %d\n", ret);
			ret = 1;
			goto out;
		}
		printf("sending %f (%zu bytes)\n", ((float*)mem2ptr)[0], sizeof(float));
		// Initiate write transfer
		if ((ret = villasfpga_write(vh, mem2, sizeof(float))) != 0) {
			fprintf(stderr, "Failed to initiate write transfer\n");
			ret = 1;
			goto out;
		}

		if ((ret = villasfpga_write_complete(vh, &size)) != 0) {
			fprintf(stderr, "Failed to write complete\n");
			ret = 1;
			goto out;
		}

		if ((ret = villasfpga_read_complete(vh, &size)) != 0) {
			fprintf(stderr, "Failed to write complete\n");
			ret = 1;
			goto out;
		}

		printf("Read %f (%zu bytes)\n", ((float*)mem1ptr)[0], size);
	}

	ret = 0;
 out:
	return ret;
}
