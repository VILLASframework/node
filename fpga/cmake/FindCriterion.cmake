# Try to find Criterion
#
# Once done this will define
#  CRITERION_FOUND - System has Criterion
#  CRITERION_INCLUDE_DIRS - The Criterion include directories
#  CRITERION_LIBRARIES - The libraries needed to use Criterion
#
# SPDX-FileCopyrightText: 2017 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: WTFPL

find_package(PkgConfig)

find_path(CRITERION_INCLUDE_DIR criterion/criterion.h
          PATH_SUFFIXES criterion)

find_library(CRITERION_LIBRARY NAMES criterion libcriterion)

set(CRITERION_LIBRARIES ${CRITERION_LIBRARY})
set(CRITERION_INCLUDE_DIRS ${CRITERION_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)

# Handle the QUIET and REQUIRED arguments and set CRITERION_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(Criterion DEFAULT_MSG
                                  CRITERION_LIBRARY CRITERION_INCLUDE_DIR)

mark_as_advanced(CRITERION_INCLUDE_DIR CRITERION_LIBRARY)
