# CMakeLists.txt.
#
# Author: Jitpanu Maneeratpongsuk <jitpanu.maneeratpongsuk@rwth-aachen.de>
# SPDX-FileCopyrightText: 2025 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

find_path(OPENDSSC_INCLUDE_DIR
    NAMES OpenDSSCDLL.h
    PATH_SUFFIXES OpenDSSC
)

find_library(OPENDSSC_LIBRARY_DIR
    NAMES OpenDSSC
    PATH_SUFFIXES openDSSC/bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenDSSC DEFAULT_MSG OPENDSSC_LIBRARY_DIR OPENDSSC_INCLUDE_DIR)

mark_as_advanced(OPENDSSC_INCLUDE_DIR OPENDSS_LIBRARY_DIR)

if(OpenDSSC_FOUND)
    add_library(OpenDSSC SHARED IMPORTED)
    set_target_properties(OpenDSSC PROPERTIES
        IMPORTED_LOCATION "${OPENDSSC_LIBRARY_DIR}"
        INTERFACE_INCLUDE_DIRECTORIES "${OPENDSSC_INCLUDE_DIR}"
        INTERFACE_COMPILE_OPTIONS "-D_D2C_SYSFILE_H_LONG_IS_INT64"
    )
endif()
