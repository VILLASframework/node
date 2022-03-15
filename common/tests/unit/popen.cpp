/** Unit tests for bi-directional popen
 *
 * @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
 * @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

// cppcheck-suppress unknownMacro
TestSuite(popen, .description = "Bi-directional popen");

Test(popen, no_shell)
{
	PopenStream proc("/usr/bin/tee", {"tee", "test"});

	proc.cout() << "Hello World" << std::endl;
	proc.cout().flush();

	std::string str, str2;

	proc.cin() >> str >> str2;

	std::cout << str << str2 << std::endl;

	cr_assert_eq(str, "Hello");
	cr_assert_eq(str2, "World");

	proc.kill();
	proc.close();
}

Test(popen, shell)
{
	PopenStream proc("echo \"Hello World\"", {}, {}, std::string(), true);

	std::string str, str2;

	proc.cin() >> str >> str2;

	cr_assert_eq(str, "Hello");
	cr_assert_eq(str2, "World");

	proc.kill();
	proc.close();
}

Test(popen, wd)
{
	PopenStream proc("/usr/bin/pwd", {"pwd"}, {}, "/usr/lib");

	std::string wd;

	proc.cin() >> wd;

	cr_assert_eq(wd, "/usr/lib");

	proc.kill();
	proc.close();
}

Test(popen, env)
{
	PopenStream proc("echo $MYVAR", {}, {{"MYVAR", "TESTVAL"}}, std::string(), true);

	std::string var;

	proc.cin() >> var;

	cr_assert_eq(var, "TESTVAL");

	proc.kill();
	proc.close();
}
