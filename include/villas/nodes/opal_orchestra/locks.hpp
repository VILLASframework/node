/* Helpers for RTAPI locking primitives
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <mutex>

#include <villas/nodes/opal_orchestra/error.hpp>

extern "C" {
#include <RTAPI.h>
}

namespace villas {
namespace node {
namespace orchestra {

class RTConnectionLockGuard : std::lock_guard<std::mutex> {
public:
  RTConnectionLockGuard(unsigned int connectionKey);
};

template <RTAPIReturn_t (*lock)(), RTAPIReturn_t (*unlock)()>
class RTLockGuard : RTConnectionLockGuard {
public:
  RTLockGuard(unsigned int connectionKey)
      : RTConnectionLockGuard(connectionKey) {

    auto ret = lock();
    if (ret != RTAPI_SUCCESS) {
      throw RTError(ret, "Failed to lock Orchestra");
    }
  }

  ~RTLockGuard() { unlock(); }
};

using RTPublishLockGuard = RTLockGuard<RTPublishLock, RTPublishUnlock>;
using RTSubscribeLockGuard = RTLockGuard<RTSubscribeLock, RTSubscribeUnlock>;

} // namespace orchestra
} // namespace node
} // namespace villas
