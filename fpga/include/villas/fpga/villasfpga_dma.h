/** C bindings for VILLASfpga
 *
 * Author: Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-FileCopyrightText: 2023 Niklas Eiling <niklas.eiling@eonerc.rwth-aachen.de>
 * SPDX-License-Identifier: Apache-2.0
 ******************************************************************************/

#pragma once

extern "C" {

#include <stddef.h>

typedef struct villasfpga_handle_t *villasfpga_handle;
typedef struct villasfpga_memory_t *villasfpga_memory;

villasfpga_handle villasfpga_init(const char *configFile);

void villasfpga_destroy(villasfpga_handle handle);

int villasfpga_alloc(villasfpga_handle handle, villasfpga_memory *mem, size_t size);
int villasfpga_register(villasfpga_handle handle, villasfpga_memory *mem);
int villasfpga_free(villasfpga_memory mem);

int villasfpga_read(villasfpga_handle handle, villasfpga_memory mem, size_t size);
int vilalsfpga_read_complete(villasfpga_handle handle, size_t *size);

int villasfpga_write(villasfpga_handle handle, villasfpga_memory mem, size_t size);
int vilalsfpga_write_complete(villasfpga_handle handle, size_t *size);

} // extern "C"
