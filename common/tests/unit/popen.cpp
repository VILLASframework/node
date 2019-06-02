/** Unit tests for bi-directional popen
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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

#include <iostream>
#include <criterion/criterion.h>

#include <villas/popen.hpp>

using namespace villas::utils;

TestSuite(popen, .description = "Bi-directional popen");

Test(popen, cat)
{
	Popen proc("cat");

	proc.out() << "Hello World" << std::endl;
	proc.out().flush();

	std::string str, str2;

	proc.in() >> str >> str2;

	cr_assert_eq(str, "Hello");
	cr_assert_eq(str2, "World");

	proc.kill();
	proc.close();
}
