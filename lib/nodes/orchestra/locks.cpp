/* Helpers for RTAPI locking primitives
 *
 * Author: Steffen Vogel <steffen.vogel@opal-rt.com>
 *
 * SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
 * SPDX-License-Identifier: Apache-2.0
 */

#include <mutex>

extern "C" {
#include <RTAPI.h>
}

#include <villas/nodes/orchestra/locks.hpp>

// The Orchestra RTAPI is a not thread-safe and also provides no rentrant API.
// Hence, we are limited here to a single connection to the Orchestra Framework which is guarded by the global lock.
static std::mutex globalLock;

using namespace villas::node::orchestra;

RTConnectionLockGuard::RTConnectionLockGuard(unsigned int connectionKey)
    : std::lock_guard<std::mutex>(globalLock) {
  RTSetConnectionKey(connectionKey);
}
