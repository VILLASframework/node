# CMakeLists.
#
# @author Steffen Vogel <stvogel@eonerc.rwth-aachen.de>
# @copyright 2014-2019, Institute for Automation of Complex Power Systems, EONERC
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
    average.cpp
    cast.cpp
    decimate.cpp
    #dp.cpp
    drop.cpp
    dump.cpp
    ebm.cpp
    fix.cpp
    jitter_calc.cpp
    limit_rate.cpp
    restart.cpp
    scale.cpp
    shift_seq.cpp
    shift_ts.cpp
    skip_first.cpp
    stats.cpp
    ts.cpp
    pps_ts.cpp
    print.cpp
)

add_library(hooks STATIC ${HOOK_SRC})
target_include_directories(hooks PUBLIC ${INCLUDE_DIRS})
target_link_libraries(hooks INTERFACE ${LIBRARIES} PUBLIC villas-common)
