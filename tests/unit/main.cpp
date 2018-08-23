/** Custom main() for Criterion
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Institute for Automation of Complex Power Systems, EONERC
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

#include <criterion/criterion.h>
#include <criterion/logging.h>

#include <villas/log.h>
#include <villas/memory.h>
#include <villas/node/config.h>

void init_memory()
{
	memory_init(DEFAULT_NR_HUGEPAGES);
}

int main(int argc, char *argv[])
{
	int ret;

	struct criterion_test_set *tests;
	struct log log;

	ret = log_init(&log, "test_logger", 2, LOG_ALL);
	if (ret)
		error("Failed to initialize logging sub-system");

	ret = log_open(&log);
	if (ret)
		error("Failed to start logging sub-system");

	/* Run criterion tests */
	tests = criterion_initialize();

	int result = 0;
	if (criterion_handle_args(argc, argv, true))
		result = !criterion_run_all_tests(tests);

	criterion_finalize(tests);

	return result;
}
