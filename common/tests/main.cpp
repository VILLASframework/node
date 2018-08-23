/** Main Unit Test entry point.
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2017-2018, Steffen Vogel
 * @license GNU General Public License (version 3)
 *
 * VILLAScommon
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
#include <criterion/options.h>

#include <villas/log.hpp>

#include <spdlog/spdlog.h>

void init_logging();

int main(int argc, char *argv[])
{
	int ret;

	init_logging();

	/* Run criterion tests */
	auto tests = criterion_initialize();

	ret = criterion_handle_args(argc, argv, true);
	if (ret)
		ret = !criterion_run_all_tests(tests);

	criterion_finalize(tests);

	return ret;
}
