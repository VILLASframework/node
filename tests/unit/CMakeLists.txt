# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

set(TEST_SRC
    config_json.cpp
    config.cpp
    format.cpp
    helpers.cpp
    json.cpp
    main.cpp
    mapping.cpp
    memory.cpp
    pool.cpp
    queue_signalled.cpp
    queue.cpp
    signal.cpp
)

add_executable(unit-tests ${TEST_SRC})
target_link_libraries(unit-tests PUBLIC
    PkgConfig::CRITERION
    Threads::Threads
    villas
)

add_custom_target(run-unit-tests
    COMMAND
        /bin/bash -o pipefail -c \"
            $<TARGET_FILE:unit-tests> 2>&1 | c++filt\"
    DEPENDS
        unit-tests
    USES_TERMINAL
)

add_custom_target(run-python-binding-tests
    COMMAND
        python3 ${CMAKE_SOURCE_DIR}/tests/unit/python_binding.py
    DEPENDS
        python-binding
    USES_TERMINAL
)

add_dependencies(tests unit-tests)
add_dependencies(run-tests run-unit-tests run-python-binding-tests)
