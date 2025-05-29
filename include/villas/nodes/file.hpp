/* Node type: File.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cstddef>
#include <cstdio>

#include <vector>
#include <villas/format.hpp>
#include <villas/task.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;

#define FILE_MAX_PATHLEN 512

struct file {
  Format *formatter;
  FILE *stream_in;
  FILE *stream_out;

  char *uri_tmpl; // Format string for file name.
  char *uri;      // Real file name.

  unsigned skip_lines; // Skip the first n-th lines/samples of the file.
  int flush;           // Flush / upload file contents after each write.
  struct Task
      task; // Timer file descriptor. Blocks until 1 / rate seconds are elapsed.
  double rate; // The read rate.
  size_t
      buffer_size_out; // Defines size of output stream buffer. No buffer is created if value is set to zero.
  size_t
      buffer_size_in; // Defines size of input stream buffer. No buffer is created if value is set to zero.

  enum class EpochMode {
    DIRECT,
    WAIT,
    RELATIVE,
    ABSOLUTE,
    ORIGINAL
  } epoch_mode; // Specifies how file::offset is calculated.

  enum class EOFBehaviour {
    STOP,   // Terminate when EOF is reached.
    REWIND, // Rewind the file when EOF is reached.
    SUSPEND // Blocking wait when EOF is reached.
  } eof_mode;

  struct timespec
      first; // The first timestamp in the file file::{read,write}::uri
  struct timespec epoch; // The epoch timestamp from the configuration.
  struct timespec
      offset; // An offset between the timestamp in the input file and the current time

  std::vector<Sample *> samples;
  size_t read_pos;

  enum class ReadMode { RATE_BASED, READ_ALL } read_mode;
  bool read;
};

char *file_print(NodeCompat *n);

int file_parse(NodeCompat *n, json_t *json);

int file_start(NodeCompat *n);

int file_stop(NodeCompat *n);

int file_init(NodeCompat *n);

int file_destroy(NodeCompat *n);

int file_poll_fds(NodeCompat *n, int fds[]);

int file_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int file_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

} // namespace node
} // namespace villas
