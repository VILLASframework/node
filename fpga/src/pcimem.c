/** Simple program to read/write from/to a pci device from userspace.
 *
 *  @Copyright 2010, Bill Farrow (bfarrow@beyondelectronics.us)
 *
 *  Based on the devmem2.c code
 *  @copyright 2000, Jan-Derk Bakker (J.D.Bakker@its.tudelft.nl)
 *
 * VILLASfpga
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *********************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <ctype.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/mman.h>

#define PRINT_ERROR \
	do { \
		fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
		__LINE__, __FILE__, errno, strerror(errno)); exit(1); \
	} while(0)

#define MAP_SIZE 4096UL
#define MAP_MASK (MAP_SIZE - 1)

int main(int argc, char **argv) {
	int fd;
	void *map_base, *virt_addr;
	unsigned read_result, writeval;
	char *filename;
	off_t target;
	int access_type = 'w';

	if (argc < 3) {
		// pcimem /sys/bus/pci/devices/0001\:00\:07.0/resource0 0x100 w 0x00
		// argv[0]  [1]                                         [2]   [3] [4]
		fprintf(stderr, "\nUsage:\t%s { sys file } { offset } [ type [ data ] ]\n"
			"\tsys file: sysfs file for the pci resource to act on\n"
			"\toffset  : offset into pci memory region to act upon\n"
			"\ttype    : access operation type : [b]yte, [h]alfword, [w]ord\n"
			"\tdata    : data to be written\n\n",
			argv[0]);
		exit(1);
	}

	filename = argv[1];
	target = strtoul(argv[2], 0, 0);

	if(argc > 3)
		access_type = tolower(argv[3][0]);

	fd = open(filename, O_RDWR | O_SYNC);
	if (fd < 0)
		PRINT_ERROR;

	printf("%s opened.\n", filename);
	printf("Target offset is %#lx, page size is %lu\n", target, sysconf(_SC_PAGE_SIZE));

	fflush(stdout);

	/* Map one page */
	printf("mmap(%d, %lu, %#x, %#x, %d, %#lx)\n", 0,
		MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED,
		fd, target);

	map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, target & ~MAP_MASK);

	if(map_base == (void *) -1)
		PRINT_ERROR;

	printf("PCI Memory mapped to address %p.\n", map_base);
	fflush(stdout);

	virt_addr = map_base + (target & MAP_MASK);

	switch(access_type) {
		case 'b':
			read_result = *((unsigned char *) virt_addr);
			break;
		case 'h':
			read_result = *((unsigned short *) virt_addr);
			break;
		case 'w':
			read_result = *((unsigned int *) virt_addr);
			break;
		default:
			fprintf(stderr, "Illegal data type '%c'.\n", access_type);
			exit(2);
	}

	printf("Value at offset %#lx (%p): %#x\n", target, virt_addr, read_result);
	fflush(stdout);

	if(argc > 4) {
		writeval = strtoul(argv[4], 0, 0);
		switch(access_type) {
			case 'b':
				*((unsigned char *) virt_addr) = writeval;
				read_result = *((unsigned char *) virt_addr);
				break;
			case 'h':
				*((unsigned short *) virt_addr) = writeval;
				read_result = *((unsigned short *) virt_addr);
				break;
			case 'w':
				*((unsigned int *) virt_addr) = writeval;
				read_result = *((unsigned int *) virt_addr);
				break;
		}
		printf("Written %#x; readback %#x\n", writeval, read_result);
		fflush(stdout);
	}

	if(munmap(map_base, MAP_SIZE) == -1)
		PRINT_ERROR;

	close(fd);

	return 0;
}

