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

#include <villas/nodes/opal_orchestra/locks.hpp>

// Global lock for access to OPAL-RT's RTAPI Orchestra API.
// The Orchestra RTAPI is non-thread-safe and non-reentrant.
// Hence, this mutex is needed for mediating access to the API
// by a single thread at a time.
static std::mutex globalLock;

using namespace villas::node::orchestra;

RTConnectionLockGuard::RTConnectionLockGuard(unsigned int connectionKey)
    : std::lock_guard<std::mutex>(globalLock) {
  RTSetConnectionKey(connectionKey);
}
