# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set(TEST_SRC
    base64.cpp
    buffer.cpp
    config_json.cpp
    config.cpp
    format.cpp
    helpers.cpp
    hist.cpp
    json.cpp
    kernel.cpp
    list.cpp
    main.cpp
    mapping.cpp
    memory.cpp
    pool.cpp
    popen.cpp
    queue_signalled.cpp
    queue.cpp
    signal.cpp
    task.cpp
    timing.cpp
    utils.cpp
)

if(WITH_NODE_C37_118)
    list(APPEND TEST_SRC c37_118.cpp)
endif()

add_executable(unit-tests ${TEST_SRC})
target_link_libraries(unit-tests PUBLIC
    PkgConfig::CRITERION
    Threads::Threads
    nodes
    villas
)

add_custom_target(run-unit-tests
    COMMAND
        /usr/bin/env bash -o pipefail -c \"
            $<TARGET_FILE:unit-tests> 2>&1 | c++filt\"
    DEPENDS
        unit-tests
    USES_TERMINAL
)

add_dependencies(tests unit-tests)
add_dependencies(run-tests run-unit-tests)
