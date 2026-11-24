/* An exponential window.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/dsp/window.hpp>

namespace villas {
namespace dsp {

template <typename T> class ExponentialWindow {

protected:
  T a;
  T last;

public:
  ExponentialWindow(T _a, T i = 0) : a(a), last(i) {}

  T update(T in) {
    last = a * in + (1 - a) * last;

    return last;
  }
};

} // namespace dsp
} // namespace villas
