# CMakeLists.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2018, Institute for Automation of Complex Power Systems, EONERC
# @license GNU General Public License (version 3)
#
# VILLASnode
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
###################################################################################

add_executable(villas-node villas-node.cpp)
target_link_libraries(villas-node PUBLIC villas)

add_executable(villas-test-rtt villas-test-rtt.cpp)
target_link_libraries(villas-test-rtt PUBLIC villas)

add_executable(villas-test-shmem villas-test-shmem.cpp)
target_link_libraries(villas-test-shmem PUBLIC villas)

install(
    TARGETS villas-node villas-test-rtt villas-test-shmem
    COMPONENT bin
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(WITH_WEB)
    add_executable(villas-relay villas-relay.cpp)
    target_include_directories(villas-relay PRIVATE ${LIBWEBSOCKETS_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR} ${SPDLOG_INCLUDE_DIRS})
    target_link_libraries(villas-relay PRIVATE websockets_shared)

    install(
        TARGETS villas-relay
        COMPONENT bin
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(WITH_IO)
    add_executable(villas-test-cmp villas-test-cmp.cpp)
    target_link_libraries(villas-test-cmp PUBLIC villas)

    add_executable(villas-convert villas-convert.cpp)
    target_link_libraries(villas-convert PUBLIC villas)

    add_executable(villas-pipe villas-pipe.cpp)
    target_link_libraries(villas-pipe PUBLIC villas)

    add_executable(villas-signal villas-signal.cpp)
    target_link_libraries(villas-signal PUBLIC villas)

    install(
        TARGETS villas-convert villas-pipe villas-signal villas-test-cmp
        COMPONENT bin
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

if(WITH_IO AND WITH_HOOKS)
    add_executable(villas-hook villas-hook.cpp)
    target_link_libraries(villas-hook PUBLIC villas)

    install(
        TARGETS villas-hook
        COMPONENT bin
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

