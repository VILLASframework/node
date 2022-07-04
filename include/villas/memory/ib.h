/** Memory allocators.
 *
 * @file
 * @author Dennis Potter <dennis@dennispotter.eu>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#pragma once

#include <villas/node.hpp>

namespace villas {
namespace node {
namespace memory {

struct IB {
	struct ibv_pd *pd;
	struct Type *parent;
};

struct ibv_mr * ib_get_mr(void *ptr);

} /* namespace memory */
} /* namespace node */
} /* namespace villas */
