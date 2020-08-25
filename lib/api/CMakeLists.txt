
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
    request.cpp
    response.cpp

    requests/status.cpp
    requests/capabiltities.cpp
    requests/config.cpp
    requests/shutdown.cpp
    requests/restart.cpp
    requests/nodes.cpp
    requests/node_info.cpp
    requests/node_action.cpp
    requests/node_stats.cpp
    requests/node_stats_reset.cpp
    requests/node_file.cpp
    requests/paths.cpp
	requests/path_info.cpp
    requests/path_action.cpp
)

add_library(api STATIC ${API_SRC})
target_include_directories(api PUBLIC ${INCLUDE_DIRS})
target_link_libraries(api PUBLIC ${LIBRARIES})
