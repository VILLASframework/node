
# CMakeLists.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2020, Institute for Automation of Complex Power Systems, EONERC
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

set(API_SRC
    session.cpp
    server.cpp

    sessions/socket.cpp

    actions/capabiltities.cpp
    actions/shutdown.cpp
    actions/status.cpp
    actions/config.cpp
    actions/nodes.cpp
    actions/paths.cpp
    actions/restart.cpp
    actions/node.cpp
    actions/stats.cpp
)

if(WITH_WEB)
    list(APPEND API_SRC
        sessions/wsi.cpp
        sessions/http.cpp
        sessions/websocket.cpp
    )
endif()

add_library(api STATIC ${API_SRC})
target_include_directories(api PUBLIC ${INCLUDE_DIRS})
target_link_libraries(api PUBLIC ${LIBRARIES})

