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

set(HOOK_SRC
    convert.c
    decimate.c
    drop.c
    jitter_calc.c
    map.c
    restart.c
    shift_seq.c
    shift_ts.c
    skip_first.c
    stats.c
    ts.c
    limit_rate.c
    scale.c
)

if(WITH_IO)
    list(APPEND HOOK_SRC
        print.c
    )
endif()

add_library(hooks STATIC ${HOOK_SRC})
target_include_directories(hooks PUBLIC ${INCLUDE_DIRS})
target_link_libraries(hooks INTERFACE ${LIBRARIES})
