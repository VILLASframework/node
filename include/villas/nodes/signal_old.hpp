/* Node-type for signal generation.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/task.hpp>
#include <villas/timing.hpp>

namespace villas {
namespace node {

// Forward declarations
class NodeCompat;
struct Sample;

struct signal_node {
  struct Task task; // Timer for periodic events.
  int rt;           // Real-time mode?

  enum class SignalType {
    RANDOM,
    SINE,
    SQUARE,
    TRIANGLE,
    RAMP,
    COUNTER,
    CONSTANT,
    MIXED,
    PULSE
  } *
      type; // Signal type

  double rate;       // Sampling rate.
  double *frequency; // Frequency of the generated signals.
  double *amplitude; // Amplitude of the generated signals.
  double *stddev; // Standard deviation of random signals (normal distributed).
  double *offset; // A constant bias.
  double *
      pulse_width; // Width of a pulse with respect to the rate (duration = pulse_width/rate)
  double *pulse_low;  // Amplitude when pulse signal is off
  double *pulse_high; // Amplitude when pulse signal is on
  double *phase;      // Phase (rad) offset with respect to program start
  int monitor_missed; // Boolean, if set, node counts missed steps and warns user.

  double *
      last; // The values from the previous period which are required for random walk.

  unsigned values; // The number of values which will be emitted by this node.
  int limit; // The number of values which should be generated by this node. <0 for infinitve.

  struct timespec started; // Point in time when this node was started.
  unsigned counter;        // The number of packets already emitted.
  unsigned missed_steps;   // Total number of missed steps.
};

char *signal_node_print(NodeCompat *n);

int signal_node_parse(NodeCompat *n, json_t *json);

int signal_node_start(NodeCompat *n);

int signal_node_stop(NodeCompat *n);

int signal_node_init(NodeCompat *n);

int signal_node_destroy(NodeCompat *n);

int signal_node_prepare(NodeCompat *n);

int signal_node_read(NodeCompat *n, struct Sample *const smps[], unsigned cnt);

int signal_node_poll_fds(NodeCompat *n, int fds[]);

} // namespace node
} // namespace villas
