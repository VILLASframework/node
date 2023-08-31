/* Custom main() for Criterion
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <villas/node/memory.hpp>

using namespace villas::node;

void init_memory()
{
	int ret __attribute__((unused));
	ret = memory::init(DEFAULT_NR_HUGEPAGES);
}
