# CMakeLists.txt.
#
# Author: Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
# SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set(OPENDSS_INCLUDE_DIRS "/usr/local/openDSSC/bin/")
find_library(OPENDSS_LIBRARY NAMES OpenDSSC PATHS ${OPENDSS_INCLUDE_DIRS})
