/* Memory allocators.
 *
 * Author: Dennis Potter <dennis@dennispotter.eu>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <villas/node.hpp>

namespace villas {
namespace node {
namespace memory {

struct IB {
  struct ibv_pd *pd;
  struct Type *parent;
};

struct ibv_mr *ib_get_mr(void *ptr);

} // namespace memory
} // namespace node
} // namespace villas
