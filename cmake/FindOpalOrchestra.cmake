# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2025, OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0

find_path(OPAL_ORCHESTRA_INCLUDE_DIR
    NAMES RTAPI.h
    PATHS
        /usr/opalrt/common/bin
)

find_library(OPAL_ORCHESTRA_LIBRARY
    NAMES OpalOrchestra
    PATHS
        /usr/opalrt/common/include
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpalOrchestra DEFAULT_MSG OPAL_ORCHESTRA_LIBRARY OPAL_ORCHESTRA_INCLUDE_DIR)

mark_as_advanced(OPAL_ORCHESTRA_INCLUDE_DIR OPAL_ORCHESTRA_LIBRARY)

set(OPAL_ORCHESTRA_LIBRARIES ${OPAL_ORCHESTRA_LIBRARY})
set(OPAL_ORCHESTRA_INCLUDE_DIRS ${OPAL_ORCHESTRA_INCLUDE_DIR})

add_library(OpalOrchestra INTERFACE)
target_link_libraries(OpalOrchestra INTERFACE ${OPAL_ORCHESTRA_LIBRARY})
target_include_directories(OpalOrchestra INTERFACE ${OPAL_ORCHESTRA_INCLUDE_DIR})
