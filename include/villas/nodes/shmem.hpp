/* Node-type for shared memory communication.
 *
 * Author: Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/node/config.hpp>
#include <villas/node/memory.hpp>
#include <villas/pool.hpp>
#include <villas/queue.h>
#include <villas/shmem.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

struct shmem {
  const char *out_name;       // Name of the shm object for the output queue.
  const char *in_name;        // Name of the shm object for the input queue.
  struct ShmemConfig conf;    // Interface configuration struct.
  char **exec;                // External program to execute on start.
  struct ShmemInterface intf; // Shmem interface
};

char *shmem_print(NodeCompat *n);

int shmem_parse(NodeCompat *n, json_t *json);

int shmem_start(NodeCompat *n);

int shmem_stop(NodeCompat *n);

int shmem_init(NodeCompat *n);

int shmem_prepare(NodeCompat *n);

int shmem_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int shmem_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

} // namespace node
} // namespace villas
