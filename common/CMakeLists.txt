## CMakeLists.txt
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

include(FindPkgConfig)
include(FeatureSummary)
include(GNUInstallDirs)

# Check packages
find_package(OpenSSL 1.0.0 REQUIRED)
find_package(CURL 7.29 REQUIRED)
find_package(spdlog 1.6.0 REQUIRED)
find_package(fmt 6.0.0 REQUIRED)

pkg_check_modules(JANSSON IMPORTED_TARGET REQUIRED jansson>=2.7)
pkg_check_modules(LIBCONFIG IMPORTED_TARGET libconfig>=1.4.9)
pkg_check_modules(UUID IMPORTED_TARGET REQUIRED uuid>=2.23)

if(fmt_VERSION VERSION_LESS "9.0.0")
    message("Using legacy ostream formatting")
    set(FMT_LEGACY_OSTREAM_FORMATTER 1)
endif()

add_subdirectory(lib)
if(WITH_TESTS)
    add_subdirectory(tests)
endif()

# Disable any colored log output
option(LOG_COLOR_DISABLE "Disable any colored log output" OFF)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/include/villas/config.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/include/villas/config.hpp
)
