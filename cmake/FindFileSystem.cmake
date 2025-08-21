# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2025 Steffen Vogel <steffen.vogel@opal-rt.com>
# SPDX-License-Identifier: Apache-2.0

set(FILESYSTEM_TEST_CODE "
#include <filesystem>

int main(void) {
	return std::filesystem::is_regular_file(\"/\") ? 0 : 1;
}
")

include(CheckCXXSourceCompiles)
include(CMakePushCheckState)

cmake_push_check_state(RESET)
check_cxx_source_compiles("${FILESYSTEM_TEST_CODE}" CXX17_FILESYSTEM)
cmake_pop_check_state()

unset(FILESYSTEM_TEST_CODE)
