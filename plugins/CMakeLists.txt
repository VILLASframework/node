# CMakeLists.txt.
#
# Author: Steffen Vogel <post@steffenvogel.de>
# SPDX-FileCopyrightText: 2014-2023 Institute for Automation of Complex Power Systems, RWTH Aachen University
# SPDX-License-Identifier: Apache-2.0

# Plugins
link_libraries(villas)
add_definitions("-DVILLAS")

if(WITH_HOOKS)
    add_library(example_hook MODULE example_hook.cpp)
    install(
        TARGETS example_hook
        COMPONENT plugins
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBEXECDIR}/villas/node/plugins
    )
endif()
