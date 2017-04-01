/** Custom main() for Criterion
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2016, Institute for Automation of Complex Power Systems, EONERC
 *   This file is part of VILLASnode. All Rights Reserved. Proprietary and confidential.
 *   Unauthorized copying of this file, via any medium is strictly prohibited.
 *********************************************************************************/

#include <criterion/criterion.h>
#include <criterion/logging.h>

#include <villas/log.h>
#include <villas/memory.h>

#include "config.h"

int main(int argc, char *argv[])
{
	int ret;

	struct criterion_test_set *tests;
	struct log log;

	ret = log_init(&log, V, LOG_ALL);
	if (ret) {
		error("Failed to initialize logging sub-system");
		return ret;
	}
	
	ret = memory_init(DEFAULT_NR_HUGEPAGES);
	if (ret) {
		error("Failed to initialize memory sub-system");
		return ret;
	}

	/* Run criterion tests */
	tests = criterion_initialize();
	
	ret = criterion_handle_args(argc, argv, true);
	if (ret)
		ret = !criterion_run_all_tests(tests);

	criterion_finalize(tests);

	return ret;
}