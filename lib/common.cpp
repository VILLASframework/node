/* Common code.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cstdlib>

#include <villas/common.hpp>

std::string stateToString(enum State s) {
  switch (s) {
  case State::DESTROYED:
    return "destroyed";

  case State::INITIALIZED:
    return "initialized";

  case State::PARSED:
    return "parsed";

  case State::CHECKED:
    return "checked";

  case State::STARTED:
    return "running";

  case State::STOPPED:
    return "stopped";

  case State::PENDING_CONNECT:
    return "pending-connect";

  case State::CONNECTED:
    return "connected";

  case State::PAUSED:
    return "paused";

  default:
    return "";
  }
}
