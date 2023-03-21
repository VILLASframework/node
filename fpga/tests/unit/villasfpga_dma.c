#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include <villas/fpga/villasfpga_dma.h>

int main(int argc, char *argv[])
{
	int ret;
	villasfpga_handle vh;
	villasfpga_memory mem1, mem2;
	char line[1024];
	float f;
	size_t size;

	if (argc != 2 && argv != NULL) {
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

	while (1) {
		// Setup read transfer
		if ((ret = villasfpga_read(vh, mem1, 0x200 * sizeof(uint32_t))) != 0) {
			fprintf(stderr, "Failed to initiate read transfer\n");
			ret = 1;
			goto out;
		}

		printf("Enter a float:\n");
		if ((ret = sscanf(line, "%f")) != 1) {
			fprintf(stderr, "Failed to parse input: sscanf returned %d\n", ret);
			ret = 1;
			goto out;
		}

		((float*)mem2)[0] = atof(line);
		printf("Read %f\n", ((float*)mem2)[0]);
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
		printf("Wrote %zu bytes", size);

		if ((ret = villasfpga_read_complete(vh, &size)) != 0) {
			fprintf(stderr, "Failed to write complete\n");
			ret = 1;
			goto out;
		}
		printf("Read %zu bytes", size);

		printf("Read %f\n", ((float*)mem1)[0]);
	}

	ret = 0;
 out:
	return ret;
}
