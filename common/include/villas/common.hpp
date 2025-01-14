/* Some common defines, enums and datastructures.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>

// Common states for most objects in VILLAScommon (paths, nodes, hooks, plugins)
enum class State {
  DESTROYED = 0,
  INITIALIZED = 1,
  PARSED = 2,
  CHECKED = 3,
  STARTED = 4,
  STOPPED = 5,
  PENDING_CONNECT = 6,
  CONNECTED = 7,
  PAUSED = 8,
  STARTING = 9,
  STOPPING = 10,
  PAUSING = 11,
  RESUMING = 12,
  PREPARED = 13
};

// Callback to destroy list elements.
//
// @param data A pointer to the data which should be freed.
typedef int (*dtor_cb_t)(void *);

// Convert state enum to human readable string.
std::string stateToString(enum State s);
