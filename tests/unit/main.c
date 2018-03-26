/** Custom main() for Criterion
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017, Institute for Automation of Complex Power Systems, EONERC
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
#include <villas/config.h>

int main(int argc, char *argv[])
{
	int ret;

	struct criterion_test_set *tests;
	struct log log;

	ret = log_init(&log, V, LOG_ALL);
	if (ret)
		error("Failed to initialize logging sub-system");

	ret = log_start(&log);
	if (ret)
		error("Failed to start logging sub-system");

	ret = memory_init(DEFAULT_NR_HUGEPAGES);
	if (ret)
		error("Failed to initialize memory sub-system");

	/* Run criterion tests */
	tests = criterion_initialize();

	ret = criterion_handle_args(argc, argv, true);
	if (ret)
		ret = !criterion_run_all_tests(tests);

	criterion_finalize(tests);

	return ret;
}
