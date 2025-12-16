# CMakeLists.txt
#
# Author: Steffen Vogel <steffen.vogel@opal-rt.com>
# SPDX-FileCopyrightText: 2023-2025 OPAL-RT Germany GmbH
# SPDX-License-Identifier: Apache-2.0

set(TARGET_RTLAB_ROOT "/usr/opalrt" CACHE STRING "RT-LAB Root directory")

if(EXISTS "${TARGET_RTLAB_ROOT}/common/bin/opalmodelmk")
    set(ENV{TARGET_RTLAB_ROOT} ${TARGET_RTLAB_ROOT})

    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/get_vars.mk"
        "all:\n\techo $(OPAL_LIBS) $(OPAL_LIBPATH)\n")

    execute_process(
        COMMAND make -sf ${TARGET_RTLAB_ROOT}/common/bin/opalmodelmk
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        OUTPUT_VARIABLE OPAL_VARS
    )

    string(STRIP ${OPAL_VARS} OPAL_VARS)
    string(REPLACE " " ";" OPAL_VARS ${OPAL_VARS})

    set(OPAL_LIBRARIES -lOpalCore -lOpalUtils ${OPAL_VARS} -lirc -ldl -pthread -lrt)
    set(OPAL_INCLUDE_DIR ${TARGET_RTLAB_ROOT}/common/include_target)

    add_library(OpalAsyncApi INTERFACE)
    target_include_directories(OpalAsyncApi INTERFACE ${OPAL_INCLUDE_DIR})
    target_link_libraries(OpalAsyncApi INTERFACE ${OPAL_LIBRARIES})
endif()

find_package_handle_standard_args(OpalAsyncApi DEFAULT_MSG OPAL_LIBRARIES OPAL_INCLUDE_DIR)

mark_as_advanced(OPAL_LIBRARIES OPAL_INCLUDE_DIR)
