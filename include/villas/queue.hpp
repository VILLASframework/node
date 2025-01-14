/* A simple queue protected by mutexes.
 *
 * Author: Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mutex>
#include <queue>

namespace villas {

template <typename T> class Queue {

protected:
  std::queue<T> queue;
  std::mutex mtx;

public:
  void push(T p) {
    std::unique_lock<std::mutex> guard(mtx);

    queue.push(p);
  }

  T pop() {
    std::unique_lock<std::mutex> guard(mtx);

    T res = queue.front();
    queue.pop();

    return res;
  }

  bool empty() {
    std::unique_lock<std::mutex> guard(mtx);

    return queue.empty();
  }
};

} // namespace villas
