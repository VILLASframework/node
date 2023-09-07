/* Node type: Node-type for testing Round-trip Time.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/format.hpp>
#include <villas/list.hpp>
#include <villas/task.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;
struct Sample;

struct test_rtt;

struct test_rtt_case {
  double rate;
  unsigned values;
  unsigned limit; // The number of samples we take per test.

  char *filename;
  char *filename_formatted;

  NodeCompat *node;
};

struct test_rtt {
  struct Task task;  // The periodic task for test_rtt_read()
  Format *formatter; // The format of the output file
  FILE *stream;

  double cooldown; // Number of seconds to wait beween tests.

  int current; // Index of current test in test_rtt::cases
  int counter;

  struct List cases; // List of test cases

  char *output; // The directory where we place the results.
  char *prefix; // An optional prefix in the filename.
};

char *test_rtt_print(NodeCompat *n);

int test_rtt_parse(NodeCompat *n, json_t *json);

int test_rtt_prepare(NodeCompat *n);

int test_rtt_init(NodeCompat *n);

int test_rtt_destroy(NodeCompat *n);

int test_rtt_start(NodeCompat *n);

int test_rtt_stop(NodeCompat *n);

int test_rtt_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int test_rtt_write(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int test_rtt_poll_fds(NodeCompat *n, int fds[]);

} // namespace node
} // namespace villas
