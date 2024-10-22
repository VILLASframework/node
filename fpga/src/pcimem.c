/* Simple program to read/write from/to a pci device from userspace.
 *
 * SPDX-FileCopyrightText: 2010 Bill Farrow <bfarrow@beyondelectronics.us>
 * SPDX-FileCopyrightText: 2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 *
 *  Based on the devmem2.c code
 *
 * SPDX-FileCopyrightText: 2000 Jan-Derk Bakker <J.D.Bakker@its.tudelft.nl>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define PRINT_ERROR                                                            \
  do {                                                                         \
    fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", __LINE__,         \
            __FILE__, errno, strerror(errno));                                 \
    exit(1);                                                                   \
  } while (0)

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
    fprintf(stderr,
            "\nUsage:\t%s { sys file } { offset } [ type [ data ] ]\n"
            "\tsys file: sysfs file for the pci resource to act on\n"
            "\toffset  : offset into pci memory region to act upon\n"
            "\ttype    : access operation type : [b]yte, [h]alfword, [w]ord\n"
            "\tdata    : data to be written\n\n",
            argv[0]);
    exit(1);
  }

  filename = argv[1];
  target = strtoul(argv[2], 0, 0);

  if (argc > 3)
    access_type = tolower(argv[3][0]);

  fd = open(filename, O_RDWR | O_SYNC);
  if (fd < 0)
    PRINT_ERROR;

  printf("%s opened.\n", filename);
  printf("Target offset is %#jx, page size is %lu\n", (uintmax_t)target,
         sysconf(_SC_PAGE_SIZE));

  fflush(stdout);

  // Map one page
  printf("mmap(%d, %lu, %#x, %#x, %d, %#jx)\n", 0, MAP_SIZE,
         PROT_READ | PROT_WRITE, MAP_SHARED, fd, (uintmax_t)target);

  map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd,
                  target & ~MAP_MASK);

  if (map_base == (void *)-1)
    PRINT_ERROR;

  printf("PCI Memory mapped to address %p.\n", map_base);
  fflush(stdout);

  virt_addr = (uint8_t *)map_base + (target & MAP_MASK);

  switch (access_type) {
  case 'b':
    read_result = *((unsigned char *)virt_addr);
    break;
  case 'h':
    read_result = *((unsigned short *)virt_addr);
    break;
  case 'w':
    read_result = *((unsigned int *)virt_addr);
    break;
  default:
    fprintf(stderr, "Illegal data type '%c'.\n", access_type);
    exit(2);
  }

  printf("Value at offset %#jx (%p): %#x\n", (uintmax_t)target, virt_addr,
         read_result);
  fflush(stdout);

  if (argc > 4) {
    writeval = strtoul(argv[4], 0, 0);
    switch (access_type) {
    case 'b':
      *((unsigned char *)virt_addr) = writeval;
      read_result = *((unsigned char *)virt_addr);
      break;
    case 'h':
      *((unsigned short *)virt_addr) = writeval;
      read_result = *((unsigned short *)virt_addr);
      break;
    case 'w':
      *((unsigned int *)virt_addr) = writeval;
      read_result = *((unsigned int *)virt_addr);
      break;
    }
    printf("Written %#x; readback %#x\n", writeval, read_result);
    fflush(stdout);
  }

  if (munmap(map_base, MAP_SIZE) == -1)
    PRINT_ERROR;

  close(fd);

  return 0;
}
