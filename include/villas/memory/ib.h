/** Memory allocators.
 *
 * @file
 * @author Dennis Potter <dennis@dennispotter.eu>
 * @copyright 2014-2021, Institute for Automation of Complex Power Systems, EONERC
 * @license GNU General Public License (version 3)
 *
 * VILLASnode
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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
