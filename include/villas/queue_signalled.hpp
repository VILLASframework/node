/* Wrapper around queue that uses POSIX CV's for signalling writes.
 *
 * Author: Georg Martin Reinke <georg.reinke@rwth-aachen.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <condition_variable>
#include <mutex>

#include <villas/queue.hpp>

namespace villas {

template <typename T> class QueueSignalled : public Queue<T> {

private:
  std::condition_variable cv;

public:
  void push(const T &data) {
    Queue<T>::push(data);

    cv.notify_one();
  }

  T pop() {
    std::unique_lock<std::mutex> l(Queue<T>::mtx);

    while (Queue<T>::queue.empty())
      cv.wait(l);

    T res = Queue<T>::queue.front();
    Queue<T>::queue.pop();

    return res;
  }
};

} // namespace villas
