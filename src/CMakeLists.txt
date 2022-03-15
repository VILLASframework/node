# CMakeLists.
#
# @author Steffen Vogel <svogel2@eonerc.rwth-aachen.de>
# @copyright 2014-2022, Institute for Automation of Complex Power Systems, EONERC
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

set(SRCS
    villas-convert
    villas-node
    villas-pipe
    villas-signal
    villas-compare
    villas-test-config
    villas-test-rtt
)

add_executable(villas-node villas-node.cpp)
target_link_libraries(villas-node PUBLIC villas)

add_executable(villas-test-config villas-test-config.cpp)
target_link_libraries(villas-test-config PUBLIC villas)

add_executable(villas-test-rtt villas-test-rtt.cpp)
target_link_libraries(villas-test-rtt PUBLIC villas)

add_executable(villas-compare villas-compare.cpp)
target_link_libraries(villas-compare PUBLIC villas)

add_executable(villas-convert villas-convert.cpp)
target_link_libraries(villas-convert PUBLIC villas)

add_executable(villas-pipe villas-pipe.cpp)
target_link_libraries(villas-pipe PUBLIC villas Threads::Threads)

add_executable(villas-signal villas-signal.cpp)
target_link_libraries(villas-signal PUBLIC villas)

if(WITH_NODE_OPAL)
	target_include_directories(villas-node PRIVATE ${OPAL_INCLUDE_DIRS})
    target_link_libraries(villas-node PRIVATE ${OPAL_LIBRARIES})
endif()

if(WITH_GRAPHVIZ)
    add_executable(villas-graph villas-graph.cpp)
    target_link_libraries(villas-graph PUBLIC villas)
endif()

if(WITH_WEB)
    add_executable(villas-relay villas-relay.cpp)
    target_include_directories(villas-relay PRIVATE ${LIBWEBSOCKETS_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR})
    target_link_libraries(villas-relay PRIVATE PkgConfig::UUID villas)

    list(APPEND SRCS villas-relay)
endif()

if(WITH_CONFIG)
    add_executable(villas-conf2json villas-conf2json.cpp)
    target_link_libraries(villas-conf2json PUBLIC villas)

    list(APPEND SRCS villas-conf2json)
endif()

if(LIBZMQ_FOUND)
    add_executable(villas-zmq-keygen villas-zmq-keygen.cpp)
    target_link_libraries(villas-zmq-keygen PUBLIC villas-common PkgConfig::LIBZMQ)
    
    list(APPEND SRC villas-zmq-keygen)
endif()

if(WITH_HOOKS)
    add_executable(villas-hook villas-hook.cpp)
    target_link_libraries(villas-hook PUBLIC villas)

    list(APPEND SRCS villas-hook)
endif()

install(
    TARGETS ${SRCS}
    COMPONENT bin
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
