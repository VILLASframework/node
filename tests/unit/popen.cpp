/* Unit tests for bi-directional popen.
 *
 * Author: Steffen Vogel <post@steffenvogel.de>
 * SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
 * SPDX-License-Identifier: Apache-2.0
 */

#include <iostream>

#include <criterion/criterion.h>

#include <villas/popen.hpp>

using namespace villas::utils;

// cppcheck-suppress unknownMacro
TestSuite(popen, .description = "Bi-directional popen");

Test(popen, no_shell) {
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

Test(popen, shell) {
  PopenStream proc("echo \"Hello World\"", {}, {}, std::string(), true);

  std::string str, str2;

  proc.cin() >> str >> str2;

  cr_assert_eq(str, "Hello");
  cr_assert_eq(str2, "World");

  proc.kill();
  proc.close();
}

Test(popen, wd) {
  PopenStream proc("/usr/bin/pwd", {"pwd"}, {}, "/usr/lib");

  std::string wd;

  proc.cin() >> wd;

  cr_assert_eq(wd, "/usr/lib");

  proc.kill();
  proc.close();
}

Test(popen, env) {
  PopenStream proc("echo $MYVAR", {}, {{"MYVAR", "TESTVAL"}}, std::string(),
                   true);

  std::string var;

  proc.cin() >> var;

  cr_assert_eq(var, "TESTVAL");

  proc.kill();
  proc.close();
}
