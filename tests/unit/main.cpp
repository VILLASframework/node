/** Custom main() for Criterion
 *
 * @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
 * @license Apache 2.0
 *********************************************************************************/

#include <villas/node/memory.hpp>

using namespace villas::node;

void init_memory()
{
	int ret __attribute__((unused));
	ret = memory::init(DEFAULT_NR_HUGEPAGES);
}
